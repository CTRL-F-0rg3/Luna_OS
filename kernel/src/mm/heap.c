#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "spinlock.h"

#define HEAP_ALIGN 16

typedef struct heap_block {
    size_t size;              /* rozmiar payloadu, bez headera */
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

#define HEAP_HEADER_SIZE ALIGN_UP(sizeof(heap_block_t), HEAP_ALIGN)

static heap_block_t *g_head = NULL;
static heap_block_t *g_tail = NULL;
static virt_addr_t g_top = KERNEL_HEAP_BASE;
static spinlock_t g_lock = SPINLOCK_INIT;

void heap_init(void) {
    g_head = g_tail = NULL;
    g_top = KERNEL_HEAP_BASE;
    spinlock_init(&g_lock);
}

/* Rozszerza heap o co najmniej `min_total` bajtow (zaokraglone do stron),
 * mapujac nowe fizyczne strony pod adres g_top i dopisujac nowy wolny blok
 * na koniec listy. Zwraca false gdy PMM/VMM lub limit heapa sie wyczerpie. */
static bool heap_grow(size_t min_total) {
    size_t needed = (size_t)ALIGN_UP(min_total, PAGE_SIZE);

    if ((g_top + needed - KERNEL_HEAP_BASE) > KERNEL_HEAP_MAX_SIZE) {
        return false;
    }

    virt_addr_t block_virt = g_top;
    for (virt_addr_t v = block_virt; v < block_virt + needed; v += PAGE_SIZE) {
        phys_addr_t phys = pmm_alloc_page();
        if (!phys) return false;
        if (!vmm_map_page(vmm_kernel_space(), v, phys, VMM_WRITABLE | VMM_NX)) {
            pmm_free_page(phys);
            return false;
        }
    }
    g_top += needed;

    heap_block_t *block = (heap_block_t *)block_virt;
    block->size = needed - HEAP_HEADER_SIZE;
    block->free = true;
    block->next = NULL;
    block->prev = g_tail;

    if (g_tail) g_tail->next = block; else g_head = block;
    g_tail = block;

    return true;
}

static heap_block_t *find_free(size_t size) {
    for (heap_block_t *b = g_head; b; b = b->next) {
        if (b->free && b->size >= size) return b;
    }
    return NULL;
}

/* Dzieli blok `b` tak, by miec dokladnie `size` bajtow payloadu, oddajac
 * reszte jako nowy wolny blok wstawiony bezposrednio po nim. */
static void split_block(heap_block_t *b, size_t size) {
    size_t remaining = b->size - size;
    if (remaining <= HEAP_HEADER_SIZE) return; /* zbyt maly fragment, nie warto */

    heap_block_t *new_block = (heap_block_t *)((uint8_t *)b + HEAP_HEADER_SIZE + size);
    new_block->size = remaining - HEAP_HEADER_SIZE;
    new_block->free = true;
    new_block->next = b->next;
    new_block->prev = b;

    if (b->next) b->next->prev = new_block; else g_tail = new_block;
    b->next = new_block;
    b->size = size;
}

/* Scala `b` z nastepnym blokiem jesli tez jest wolny, potem probuje scalic
 * w tyl (rekurencyjnie w lewo) - w efekcie scala caly ciagly wolny obszar. */
static void coalesce(heap_block_t *b) {
    if (b->next && b->next->free) {
        b->size += HEAP_HEADER_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b; else g_tail = b;
    }
    if (b->prev && b->prev->free) {
        coalesce(b->prev);
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size_t)ALIGN_UP(size, HEAP_ALIGN);

    spinlock_acquire(&g_lock);

    heap_block_t *b = find_free(size);
    if (!b) {
        if (!heap_grow(size + HEAP_HEADER_SIZE)) {
            spinlock_release(&g_lock);
            return NULL;
        }
        b = find_free(size);
        if (!b) { spinlock_release(&g_lock); return NULL; }
    }

    split_block(b, size);
    b->free = false;

    spinlock_release(&g_lock);
    return (void *)((uint8_t *)b + HEAP_HEADER_SIZE);
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *b = (heap_block_t *)((uint8_t *)ptr - HEAP_HEADER_SIZE);

    spinlock_acquire(&g_lock);
    b->free = true;
    coalesce(b);
    spinlock_release(&g_lock);
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    heap_block_t *b = (heap_block_t *)((uint8_t *)ptr - HEAP_HEADER_SIZE);
    if (b->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    for (size_t i = 0; i < b->size; i++) dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

/* Alokacja z podniesionym wyrownaniem: bierzemy zapas na wyrownanie +
 * miejsce na wskaznik do oryginalnego bloku, zapisany bezposrednio przed
 * zwracanym adresem. kfree_aligned() odczytuje ten wskaznik i zwalnia
 * prawdziwy (nie-wyrownany) blok. */
void *kmalloc_aligned(size_t size, size_t align) {
    if (align <= HEAP_ALIGN) return kmalloc(size);

    size_t total = size + align + sizeof(void *);
    uint8_t *raw = (uint8_t *)kmalloc(total);
    if (!raw) return NULL;

    uint8_t *aligned = (uint8_t *)ALIGN_UP((uintptr_t)(raw + sizeof(void *)), align);
    *((void **)(aligned - sizeof(void *))) = raw;
    return aligned;
}

void kfree_aligned(void *ptr) {
    if (!ptr) return;
    void *raw = *((void **)((uint8_t *)ptr - sizeof(void *)));
    kfree(raw);
}
