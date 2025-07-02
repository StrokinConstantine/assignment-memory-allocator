#include "mem.h"
#include <assert.h>

void test_malloc_free() {
    void *heap = heap_init(5000);
    assert(heap);

    void *ptr1 = _malloc(1438);
    assert(ptr1);

    void *ptr2 = _malloc(132355);
    assert(ptr2);

    _free(ptr1);
    _free(ptr2);
}

void test_grow_heap() {
    void *heap = heap_init(40196);
    assert(heap);

    void *ptr1 = _malloc(80400);
    assert(ptr1);
}

void test_split_blocks() {
    void *heap = heap_init(4097);
    assert(heap);

    void *ptr1 = _malloc(634);
    assert(ptr1);

    void *ptr2 = _malloc(634);
    assert(ptr2);

    _free(ptr1);
    _free(ptr2);
}

int main() {
    test_malloc_free();
    test_grow_heap();
    test_split_blocks();
    printf("All tests passed.\n");
    return 0;
}
