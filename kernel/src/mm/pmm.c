#include "pmm.h"
#include "spinlock.h"

/* Metadane per-ramka: order bloku (istotny tylko gdy free=1) + flaga wolna/zajeta.
 * Indeks w tablicy = numer ramki fizycznej (adres >> PAGE_SHIFT). */
typedef struct {
    uint8_t order;
    uint8_t free;
} pmm_meta_t;

typedef struct pmm_free_node {
    struct pmm_free_node *next;
} pmm_free_node_t;

static pmm_meta_t *g_meta = NULL;
static uint64_t g_total_frames = 0;
static uint64_t g_free_frames = 0;

static pmm_free_node_t *g_free_lists[PMM_MAX_ORDER + 1];
static spinlock_t g_pmm_lock = SPINLOCK_INIT;

static inline uint64_t frame_of(phys_addr_t addr) { return addr >> PAGE_SHIFT; }

static void pmm_mark(phys_addr_t addr, uint32_t order, bool free) {
    uint64_t frame = frame_of(addr);
    g_meta[frame].order = (uint8_t)order;
    g_meta[frame].free = free ? 1 : 0;
}

static void pmm_list_push(uint32_t order, phys_addr_t addr) {
    pmm_free_node_t *node = (pmm_free_node_t *)phys_to_virt(addr);
    node->next = g_free_lists[order];
    g_free_lists[order] = node;
}

static phys_addr_t pmm_list_pop(uint32_t order) {
    pmm_free_node_t *node = g_free_lists[order];
    if (!node) return 0;
    g_free_lists[order] = node->next;
    return virt_to_phys_hhdm(node);
}

static bool pmm_list_remove(uint32_t order, phys_addr_t addr) {
    pmm_free_node_t *target = (pmm_free_node_t *)phys_to_virt(addr);
    pmm_free_node_t **cur = &g_free_lists[order];
    while (*cur) {
        if (*cur == target) {
            *cur = target->next;
            return true;
        }
        cur = &(*cur)->next;
    }
    return false;
}

/* Wstawia blok jako wolny: oznacza w meta i dodaje do free-listy danego orderu.
 * Nie robi coalescingu - uzywane przy inicjalizacji i wewnatrz pmm_free(). */
static void pmm_release_block(phys_addr_t addr, uint32_t order) {
    pmm_mark(addr, order, true);
    pmm_list_push(order, addr);
    g_free_frames += (1ULL << order);
}

/* Rozklada zakres [base, base+length) na naturalnie wyrownane bloki
 * potegowe i wstawia je jako wolne. Standardowy krok inicjalizacji buddy. */
static void pmm_add_region(phys_addr_t base, uint64_t length) {
    phys_addr_t addr = ALIGN_UP(base, PAGE_SIZE);
    phys_addr_t end = ALIGN_DOWN(base + length, PAGE_SIZE);

    while (addr < end) {
        uint32_t order = PMM_MAX_ORDER;
        while (order > 0) {
            uint64_t block_size = PAGE_SIZE << order;
            if ((addr % block_size) == 0 && addr + block_size <= end) break;
            order--;
        }
        pmm_release_block(addr, order);
        addr += (PAGE_SIZE << order);
    }
}

void pmm_init(const pmm_memmap_entry_t *entries, uint64_t entry_count, uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;

    for (uint32_t o = 0; o <= PMM_MAX_ORDER; o++) g_free_lists[o] = NULL;

    /* 1) Znajdz najwyzszy adres fizyczny wsrod WSZYSTKICH wpisow (rowniez
     *    zarezerwowanych) - metadane musza pokrywac cala przestrzen ramek,
     *    zeby indeksowanie po numerze ramki bylo zawsze bezpieczne. */
    phys_addr_t highest = 0;
    for (uint64_t i = 0; i < entry_count; i++) {
        phys_addr_t end = entries[i].base + entries[i].length;
        if (end > highest) highest = end;
    }
    g_total_frames = ALIGN_UP(highest, PAGE_SIZE) >> PAGE_SHIFT;

    uint64_t meta_size = ALIGN_UP(g_total_frames * sizeof(pmm_meta_t), PAGE_SIZE);

    /* 2) Znajdz uzywalny region, ktory pomiesci tablice metadanych, i wytnij
     *    go z tego regionu przed przekazaniem reszty do pmm_add_region(). */
    phys_addr_t meta_phys = 0;
    uint64_t chosen_idx = (uint64_t)-1;
    for (uint64_t i = 0; i < entry_count; i++) {
        if (entries[i].type != PMM_MEMMAP_USABLE) continue;
        if (entries[i].length >= meta_size) {
            meta_phys = ALIGN_UP(entries[i].base, PAGE_SIZE);
            chosen_idx = i;
            break;
        }
    }
    if (chosen_idx == (uint64_t)-1) {
        mm_panic("PMM: brak wystarczajaco duzego regionu na metadane");
    }

    g_meta = (pmm_meta_t *)phys_to_virt(meta_phys);
    for (uint64_t f = 0; f < g_total_frames; f++) {
        g_meta[f].order = 0;
        g_meta[f].free = 0; /* domyslnie wszystko "zajete" - bezpieczny default */
    }

    g_free_frames = 0;

    /* 3) Dodaj wszystkie regiony USABLE jako wolne, wycinajac fragment
     *    zuzyty przez metadane z tego jednego wybranego regionu. */
    for (uint64_t i = 0; i < entry_count; i++) {
        if (entries[i].type != PMM_MEMMAP_USABLE) continue;

        if (i == chosen_idx) {
            phys_addr_t region_end = entries[i].base + entries[i].length;
            phys_addr_t after_meta = meta_phys + meta_size;
            if (meta_phys > entries[i].base) {
                pmm_add_region(entries[i].base, meta_phys - entries[i].base);
            }
            if (after_meta < region_end) {
                pmm_add_region(after_meta, region_end - after_meta);
            }
        } else {
            pmm_add_region(entries[i].base, entries[i].length);
        }
    }

    mm_log("PMM: buddy allocator zainicjalizowany");
}

phys_addr_t pmm_alloc(uint32_t order) {
    if (order > PMM_MAX_ORDER) return 0;

    spinlock_acquire(&g_pmm_lock);

    uint32_t cur = order;
    while (cur <= PMM_MAX_ORDER && g_free_lists[cur] == NULL) cur++;
    if (cur > PMM_MAX_ORDER) {
        spinlock_release(&g_pmm_lock);
        return 0;
    }

    phys_addr_t addr = pmm_list_pop(cur);

    /* Rozbijaj wiekszy blok w dol, oddajac polowki (buddy) jako wolne na
     * kolejnych, mniejszych poziomach. */
    while (cur > order) {
        cur--;
        uint64_t half_size = PAGE_SIZE << cur;
        phys_addr_t buddy_addr = addr + half_size;
        pmm_release_block(buddy_addr, cur);
    }

    pmm_mark(addr, order, false);
    g_free_frames -= (1ULL << order);

    spinlock_release(&g_pmm_lock);
    return addr;
}

void pmm_free(phys_addr_t addr, uint32_t order) {
    spinlock_acquire(&g_pmm_lock);

    while (order < PMM_MAX_ORDER) {
        uint64_t block_size = PAGE_SIZE << order;
        phys_addr_t buddy = addr ^ block_size; /* dziala bo bloki sa naturalnie wyrownane */
        uint64_t buddy_frame = frame_of(buddy);

        if (buddy_frame >= g_total_frames) break;
        if (!g_meta[buddy_frame].free || g_meta[buddy_frame].order != order) break;
        if (!pmm_list_remove(order, buddy)) break;

        g_free_frames -= (1ULL << order); /* buddy znika z free-listy tego rzedu */
        addr = (addr < buddy) ? addr : buddy;
        order++;
    }

    pmm_release_block(addr, order);
    spinlock_release(&g_pmm_lock);
}

uint64_t pmm_total_frames(void) { return g_total_frames; }
uint64_t pmm_free_frames(void) { return __atomic_load_n(&g_free_frames, __ATOMIC_RELAXED); }
