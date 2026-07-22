#include "vmm.h"
#include "pmm.h"

#define PT_ENTRIES 512
#define PRESENT_BIT (1ULL << 0)
#define ADDR_MASK 0x000ffffffffff000ULL

static address_space_t g_kernel_space;

static inline uint64_t *table_virt(phys_addr_t phys) {
    return (uint64_t *)phys_to_virt(phys);
}

static inline uint64_t pml4_index(virt_addr_t v) { return (v >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(virt_addr_t v) { return (v >> 30) & 0x1FF; }
static inline uint64_t pd_index(virt_addr_t v)   { return (v >> 21) & 0x1FF; }
static inline uint64_t pt_index(virt_addr_t v)   { return (v >> 12) & 0x1FF; }

/* Zwraca (tworzac przy potrzebie) nastepny poziom tabeli. Nowe tabele
 * posrednie zawsze dostaja PRESENT|WRITABLE - to nie nadaje uprawnien
 * zapisu samej stronie danych, tylko pozwala na dalsza modyfikacje
 * struktury tabel; realne uprawnienia sa ustawiane na wpisie PT (4K). */
static uint64_t *get_or_create_table(uint64_t *parent, uint64_t index) {
    uint64_t entry = parent[index];
    if (entry & PRESENT_BIT) {
        return table_virt(entry & ADDR_MASK);
    }

    phys_addr_t new_table = pmm_alloc_page();
    if (!new_table) return NULL;

    uint64_t *virt = table_virt(new_table);
    for (int i = 0; i < PT_ENTRIES; i++) virt[i] = 0;

    parent[index] = new_table | PRESENT_BIT | VMM_WRITABLE;
    return virt;
}

bool vmm_map_page(address_space_t *space, virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    spinlock_acquire(&space->lock);

    uint64_t *pdpt = get_or_create_table(space->pml4, pml4_index(virt));
    uint64_t *pd = pdpt ? get_or_create_table(pdpt, pdpt_index(virt)) : NULL;
    uint64_t *pt = pd ? get_or_create_table(pd, pd_index(virt)) : NULL;

    if (!pt) {
        spinlock_release(&space->lock);
        return false;
    }

    pt[pt_index(virt)] = (phys & ADDR_MASK) | flags | PRESENT_BIT;

    spinlock_release(&space->lock);
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_unmap_page(address_space_t *space, virt_addr_t virt) {
    spinlock_acquire(&space->lock);

    uint64_t pml4e = space->pml4[pml4_index(virt)];
    if (!(pml4e & PRESENT_BIT)) { spinlock_release(&space->lock); return false; }
    uint64_t *pdpt = table_virt(pml4e & ADDR_MASK);

    uint64_t pdpte = pdpt[pdpt_index(virt)];
    if (!(pdpte & PRESENT_BIT)) { spinlock_release(&space->lock); return false; }
    uint64_t *pd = table_virt(pdpte & ADDR_MASK);

    uint64_t pde = pd[pd_index(virt)];
    if (!(pde & PRESENT_BIT)) { spinlock_release(&space->lock); return false; }
    uint64_t *pt = table_virt(pde & ADDR_MASK);

    pt[pt_index(virt)] = 0;

    spinlock_release(&space->lock);
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_map_range(address_space_t *space, virt_addr_t virt, phys_addr_t phys, uint64_t len, uint64_t flags) {
    virt_addr_t v = ALIGN_DOWN(virt, PAGE_SIZE);
    phys_addr_t p = ALIGN_DOWN(phys, PAGE_SIZE);
    virt_addr_t end = ALIGN_UP(virt + len, PAGE_SIZE);

    while (v < end) {
        if (!vmm_map_page(space, v, p, flags)) return false;
        v += PAGE_SIZE;
        p += PAGE_SIZE;
    }
    return true;
}

void vmm_activate(address_space_t *space) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(space->pml4_phys) : "memory");
}

address_space_t *vmm_kernel_space(void) { return &g_kernel_space; }

void vmm_init(uint64_t hhdm_offset, phys_addr_t kernel_phys_base, virt_addr_t kernel_virt_base) {
    phys_addr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) mm_panic("VMM: brak pamieci na PML4");

    uint64_t *pml4 = table_virt(pml4_phys);
    for (int i = 0; i < PT_ENTRIES; i++) pml4[i] = 0;

    g_kernel_space.pml4 = pml4;
    g_kernel_space.pml4_phys = pml4_phys;
    spinlock_init(&g_kernel_space.lock);

    /* 1) HHDM: caly zaadresowany fizyczny RAM, RW + NX, strony 4K.
     *    Proste i poprawne; mozna pozniej zoptymalizowac do stron 2 MiB/1 GiB. */
    uint64_t total_phys = pmm_total_frames() * PAGE_SIZE;
    if (!vmm_map_range(&g_kernel_space, hhdm_offset, 0, total_phys, VMM_WRITABLE | VMM_NX)) {
        mm_panic("VMM: mapowanie HHDM nie powiodlo sie");
    }

    /* 2) Obraz kernela z uprawnieniami per-sekcja (W^X):
     *    .text   -> R+X  (bez VMM_WRITABLE, bez VMM_NX)
     *    .rodata -> R    (bez VMM_WRITABLE, z VMM_NX)
     *    .data/.bss -> R+W (z VMM_WRITABLE, z VMM_NX) */
    uint64_t text_len   = (uint64_t)(_kernel_text_end   - _kernel_text_start);
    uint64_t rodata_len = (uint64_t)(_kernel_rodata_end - _kernel_rodata_start);
    uint64_t data_len   = (uint64_t)(_kernel_data_end   - _kernel_data_start);

    phys_addr_t text_phys   = kernel_phys_base + ((virt_addr_t)_kernel_text_start   - kernel_virt_base);
    phys_addr_t rodata_phys = kernel_phys_base + ((virt_addr_t)_kernel_rodata_start - kernel_virt_base);
    phys_addr_t data_phys   = kernel_phys_base + ((virt_addr_t)_kernel_data_start   - kernel_virt_base);

    if (text_len   && !vmm_map_range(&g_kernel_space, (virt_addr_t)_kernel_text_start,   text_phys,   text_len,   0))
        mm_panic("VMM: mapowanie .text nie powiodlo sie");
    if (rodata_len && !vmm_map_range(&g_kernel_space, (virt_addr_t)_kernel_rodata_start, rodata_phys, rodata_len, VMM_NX))
        mm_panic("VMM: mapowanie .rodata nie powiodlo sie");
    if (data_len   && !vmm_map_range(&g_kernel_space, (virt_addr_t)_kernel_data_start,   data_phys,   data_len,   VMM_WRITABLE | VMM_NX))
        mm_panic("VMM: mapowanie .data/.bss nie powiodlo sie");

    vmm_activate(&g_kernel_space);
    mm_log("VMM: wlasne tablice stron aktywne (CR3 przelaczone)");
}
