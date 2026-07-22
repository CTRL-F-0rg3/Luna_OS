#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

/* Test-and-Test-and-Set spinlock z instrukcja PAUSE w petli oczekiwania.
 * To jest baza SMP-safe: wiele rdzeni moze bezpiecznie wolac pmm_alloc/
 * pmm_free/vmm_map_page/kmalloc/kfree jednoczesnie, bez wyscigow.
 *
 * Swiadomy kompromis: jeden globalny lock na alokator (a nie per-CPU cache).
 * To poprawne i proste. Jesli w przyszlosci alokacje beda "waskim gardlem"
 * przy wielu rdzeniach jednoczesnie mocno alokujacych, mozna to rozszerzyc
 * o per-CPU freelisty z fallbackiem do globalnego alokatora - ale to
 * optymalizacja, nie wymog poprawnosci. */
typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_init(spinlock_t *lock) {
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELAXED);
}

static inline void spinlock_acquire(spinlock_t *lock) {
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
            __asm__ volatile("pause" ::: "memory");
        }
    }
}

static inline void spinlock_release(spinlock_t *lock) {
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}

#endif /* SPINLOCK_H */
