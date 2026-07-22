#ifndef MM_H
#define MM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;

#define PAGE_SIZE   0x1000ULL
#define PAGE_SHIFT  12

#define ALIGN_DOWN(x, a) ((x) & ~((uint64_t)(a) - 1))
#define ALIGN_UP(x, a)   (((x) + (uint64_t)(a) - 1) & ~((uint64_t)(a) - 1))

/* Wirtualny obszar na kernelowy heap (higher-half, poza kernelem i poza HHDM). */
#define KERNEL_HEAP_BASE     0xffffa00000000000ULL
#define KERNEL_HEAP_MAX_SIZE (256ULL * 1024 * 1024) /* 256 MiB, mozna zwiekszyc pozniej */

/* Ustawiane raz przez mm_init(), pozniej tylko odczyt */
extern uint64_t g_hhdm_offset;

static inline void *phys_to_virt(phys_addr_t p) {
    return (void *)(uintptr_t)(p + g_hhdm_offset);
}

static inline phys_addr_t virt_to_phys_hhdm(const void *v) {
    return (phys_addr_t)((uintptr_t)v - g_hhdm_offset);
}

/* Symbole z linker.ld - granice sekcji kernela (potrzebne w VMM do ustawienia
 * poprawnych uprawnien stron: .text R+X, .rodata R, .data/.bss R+W). */
extern uint8_t _kernel_text_start[], _kernel_text_end[];
extern uint8_t _kernel_rodata_start[], _kernel_rodata_end[];
extern uint8_t _kernel_data_start[], _kernel_data_end[];
extern uint8_t _kernel_start[], _kernel_end[];

/* Most do Rusta (crate::debug::log) - implementacja w mm/src/mm.c + rust FFI */
void mm_log(const char *msg);
void mm_panic(const char *msg) __attribute__((noreturn));

#endif /* MM_H */
