#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mem_internals.h"
#include "mem.h"
#include "util.h"

// Макросы
#define BLOCK_MIN_CAPACITY 24

// Вспомогательные функции
static size_t pages_count(size_t mem) {
    return mem / getpagesize() + ((mem % getpagesize()) > 0);
}

static size_t round_pages(size_t mem) {
    return getpagesize() * pages_count(mem);
}

static size_t region_actual_size(size_t query) {
    return size_max(round_pages(query), REGION_MIN_SIZE);
}

static void* map_pages(void const* addr, size_t length, int additional_flags) {
    return mmap((void*)addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags, -1, 0);
}

// Функции для работы с блоками
static void block_init(void* addr, block_size block_sz, void* next) {
    struct block_header* block = addr;
    *block = (struct block_header){
        .next = next,
        .capacity = capacity_from_size(block_sz),
        .is_free = true
    };
}

static bool block_is_big_enough(size_t query, const struct block_header* block) {
    return block->capacity.bytes >= query;
}

static void* block_after(const struct block_header* block) {
    return (void*)(block->contents + block->capacity.bytes);
}

static bool blocks_continuous(const struct block_header* fst, const struct block_header* snd) {
    return (void*)snd == block_after(fst);
}

static bool mergeable(const struct block_header* fst, const struct block_header* snd) {
    return fst->is_free && snd->is_free && blocks_continuous(fst, snd);
}

static bool try_merge_with_next(struct block_header* block) {
    struct block_header* next = block->next;
    if (!next || !mergeable(block, next)) return false;

    block_size merged_size = {
        .bytes = size_from_capacity(block->capacity).bytes + size_from_capacity(next->capacity).bytes
    };

    block_init(block, merged_size, next->next);
    return true;
}

static bool block_splittable(const struct block_header* block, size_t query) {
    return block->is_free &&
           query + offsetof(struct block_header, contents) + BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

static bool split_block(struct block_header* block, size_t query) {
    if (!block_splittable(block, query)) return false;

    size_t split_size = size_max(query, BLOCK_MIN_CAPACITY);
    block_size original_size = size_from_capacity((block_capacity){.bytes = split_size});
    block_capacity new_capacity = {
        .bytes = block->capacity.bytes - original_size.bytes
    };
    block_size new_size = size_from_capacity(new_capacity);

    block_init((uint8_t*)block + original_size.bytes, new_size, block->next);
    block->capacity.bytes = split_size;
    block->next = (void*)((uint8_t*)block + original_size.bytes);
    return true;
}

// Функции для работы с регионами
static struct region alloc_region(const void* addr, size_t query) {
    size_t actual_size = region_actual_size(size_from_capacity((block_capacity){.bytes = query}).bytes);
    void* region_addr = map_pages(addr, actual_size, MAP_FIXED_NOREPLACE);

    if (region_addr == MAP_FAILED) {
        region_addr = map_pages(NULL, actual_size, 0);
        if (region_addr == MAP_FAILED) return REGION_INVALID;
    }

    block_init(region_addr, (block_size){.bytes = actual_size}, NULL);
    return (struct region){.addr = region_addr, .size = actual_size, .extends = (region_addr == addr)};
}

// Основная логика аллокации
static struct block_search_result find_good_block(struct block_header* block, size_t query) {
    while (block) {
        if (block->is_free) {
            while (try_merge_with_next(block));
            if (block_is_big_enough(query, block)) {
                return (struct block_search_result){.type = BSR_FOUND_GOOD_BLOCK, .block = block};
            }
        }
        if (!block->next) break;
        block = block->next;
    }
    return (struct block_search_result){.type = BSR_REACHED_END_NOT_FOUND, .block = block};
}

static struct block_search_result try_allocate_existing(size_t query, struct block_header* block) {
    struct block_search_result result = find_good_block(block, query);
    if (result.type == BSR_FOUND_GOOD_BLOCK) {
        split_block(result.block, query);
        result.block->is_free = false;
    }
    return result;
}

static struct block_header* grow_heap(struct block_header* last, size_t query) {
    struct region new_region = alloc_region(block_after(last), query);
    if (region_is_invalid(&new_region)) return NULL;

    last->next = new_region.addr;
    if (new_region.extends) try_merge_with_next(last);
    return new_region.addr;
}

static struct block_header* memalloc(size_t query, struct block_header* heap_start) {
    query = size_max(query, BLOCK_MIN_CAPACITY);
    struct block_search_result result = try_allocate_existing(query, heap_start);

    if (result.type == BSR_FOUND_GOOD_BLOCK) return result.block;
    if (result.type == BSR_REACHED_END_NOT_FOUND) {
        struct block_header* new_block = grow_heap(result.block, query);
        if (!new_block) return NULL;
        return try_allocate_existing(query, new_block).block;
    }
    return NULL;
}

// API
void* _malloc(size_t query) {
    struct block_header* block = memalloc(query, (struct block_header*)HEAP_START);
    return block ? block->contents : NULL;
}

void* heap_init(size_t initial) {
    struct region region = alloc_region(HEAP_START, initial);
    return region_is_invalid(&region) ? NULL : region.addr;
}

void _free(void* mem) {
    if (!mem) return;
    struct block_header* block = block_get_header(mem);
    block->is_free = true;
    while (try_merge_with_next(block));
}
