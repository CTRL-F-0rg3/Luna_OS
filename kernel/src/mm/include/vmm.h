#ifndef VMM_H
#define VMM_H

#include "mm.h"
#include "spinlock.h"

#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_NX       (1ULL << 63)
/* Uwaga: PRESENT (bit 0) jest dodawany automatycznie przez vmm_map_page(). */

typedef struct {
    uint64_t *pml4;        /* adres wirtualny (HHDM) tablicy PML4 */
    phys_addr_t pml4_phys;
    spinlock_t lock;
} address_space_t;

/* kernel_phys_base / kernel_virt_base pochodza z limine::request::KernelAddressRequest
 * (fizyczny i wirtualny adres bazowy zaladowanego obrazu kernela). */
void vmm_init(uint64_t hhdm_offset, phys_addr_t kernel_phys_base, virt_addr_t kernel_virt_base);

address_space_t *vmm_kernel_space(void);

bool vmm_map_page(address_space_t *space, virt_addr_t virt, phys_addr_t phys, uint64_t flags);
bool vmm_unmap_page(address_space_t *space, virt_addr_t virt);
bool vmm_map_range(address_space_t *space, virt_addr_t virt, phys_addr_t phys, uint64_t len, uint64_t flags);

void vmm_activate(address_space_t *space);

#endif /* VMM_H */
