#ifndef HEAP_H
#define HEAP_H

#include "mm.h"

void heap_init(void);

void *kmalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);

/* Warianty dla wyrownan wiekszych niz domyslne (16 B) - potrzebne przez
 * Rustowy GlobalAlloc, gdy Layout::align() > 16. Kazdy wskaznik zwrocony
 * przez kmalloc_aligned MUSI byc zwolniony przez kfree_aligned, nie kfree. */
void *kmalloc_aligned(size_t size, size_t align);
void kfree_aligned(void *ptr);

#endif /* HEAP_H */
