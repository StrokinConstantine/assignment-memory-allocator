CFLAGS=--std=c17 -Wall -pedantic -Isrc/ -ggdb -Wextra -Werror -DDEBUG
BUILDDIR=build
SRCDIR=src
CC=gcc

# Целевой исполняемый файл
all: $(BUILDDIR)/main

# Создание папки build, если её нет
build:
	# Создаём директорию build
	mkdir -p $(BUILDDIR)

# Компиляция main.c
$(BUILDDIR)/main: $(BUILDDIR)/mem.o $(BUILDDIR)/util.o $(BUILDDIR)/mem_debug.o $(SRCDIR)/main.c
	# Компилируем исполняемый из object-файлов
	$(CC) $(CFLAGS) $^ -o $@

# Компиляция mem.c
$(BUILDDIR)/mem.o: $(SRCDIR)/mem.c build
	# Компилируем mem.o
	$(CC) -c $(CFLAGS) $< -o $@

# Компиляция mem_debug.c
$(BUILDDIR)/mem_debug.o: $(SRCDIR)/mem_debug.c build
	# Компилируем mem_debug.o
	$(CC) -c $(CFLAGS) $< -o $@

# Компиляция util.c
$(BUILDDIR)/util.o: $(SRCDIR)/util.c build
	# Компилируем util.o
	$(CC) -c $(CFLAGS) $< -o $@

# Очистка скомпилированных файлов
clean:
	# Удаляем build директорию
	rm -rf $(BUILDDIR)
