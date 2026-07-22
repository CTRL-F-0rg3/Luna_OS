#ifndef LUNA_MM_H
#define LUNA_MM_H

#include <stddef.h>

void* luna_alloc(size_t size);
void luna_free(void* ptr);

#endif