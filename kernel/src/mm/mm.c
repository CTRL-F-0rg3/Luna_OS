#include "mm.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"

uint64_t g_hhdm_offset = 0;

/* Zaimplementowane w Rust (kernel/src/mm/mod.rs), opakowuje crate::debug::log. */
extern void rust_debug_log(const char *msg);

void mm_log(const char *msg) {
    rust_debug_log(msg);
}

void mm_panic(const char *msg) {
    rust_debug_log(msg);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* Glowny punkt wejscia mm, wolany raz z Rusta po arch::init(). */
void mm_init(const pmm_memmap_entry_t *entries, uint64_t entry_count,
             uint64_t hhdm_offset,
             phys_addr_t kernel_phys_base, virt_addr_t kernel_virt_base) {
    pmm_init(entries, entry_count, hhdm_offset);
    vmm_init(hhdm_offset, kernel_phys_base, kernel_virt_base);
    heap_init();
    mm_log("mm: PMM + VMM + heap gotowe");
}
