#ifndef PMM_H
#define PMM_H

#include "mm.h"

/* 2^18 * 4KiB = 1 GiB - maksymalny rozmiar pojedynczego bloku buddy. */
#define PMM_MAX_ORDER 18

/* Wlasny, minimalny format wpisu mapy pamieci - odlacza C od konkretnej
 * wersji struktur crate'a `limine`. Strona Rust konwertuje odpowiedz
 * MemoryMapResponse do tablicy tych struktur i przekazuje wskaznik+len. */
typedef struct {
    phys_addr_t base;
    uint64_t length;
    uint32_t type;
} pmm_memmap_entry_t;

#define PMM_MEMMAP_USABLE                 0
#define PMM_MEMMAP_RESERVED               1
#define PMM_MEMMAP_ACPI_RECLAIMABLE       2
#define PMM_MEMMAP_ACPI_NVS               3
#define PMM_MEMMAP_BAD_MEMORY             4
#define PMM_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define PMM_MEMMAP_KERNEL_AND_MODULES     6
#define PMM_MEMMAP_FRAMEBUFFER            7

void pmm_init(const pmm_memmap_entry_t *entries, uint64_t entry_count, uint64_t hhdm_offset);

/* Zwraca adres fizyczny zaalokowanego bloku 2^order stron (0 przy braku pamieci). */
phys_addr_t pmm_alloc(uint32_t order);
void pmm_free(phys_addr_t addr, uint32_t order);

static inline phys_addr_t pmm_alloc_page(void) { return pmm_alloc(0); }
static inline void pmm_free_page(phys_addr_t addr) { pmm_free(addr, 0); }

uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);

/* Glowny punkt wejscia calego mm - w mm.c, wolany raz z Rusta. */
void mm_init(const pmm_memmap_entry_t *entries, uint64_t entry_count,
             uint64_t hhdm_offset,
             phys_addr_t kernel_phys_base, virt_addr_t kernel_virt_base);

#endif /* PMM_H */
