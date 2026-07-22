//! Most Rust <-> C dla podsystemu pamięci (kernel/src/mm/*.c).
//!
//! Sama logika (buddy PMM, VMM z page table, heap) jest napisana w C —
//! ten plik tylko:
//!   1. deklaruje sygnatury C (extern "C") i typy zgodne 1:1 z mm.h/pmm.h,
//!   2. konwertuje odpowiedź Limine (memory map, HHDM, adres kernela) do
//!      formatu, jaki rozumie C, bez alokowania niczego na heapie
//!      (heap jeszcze nie istnieje w momencie wołania init!),
//!   3. udostępnia crate::debug::log C-code przez rust_debug_log(),
//!   4. implementuje core::alloc::GlobalAlloc na bazie kmalloc/kfree,
//!      żeby Vec/Box/String działały w resztek kernela.

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::{c_char, c_void};

/// Musi być zgodne 1:1 z `pmm_memmap_entry_t` w mm/include/pmm.h.
#[repr(C)]
struct PmmMemmapEntry {
    base: u64,
    length: u64,
    r#type: u32,
}

const PMM_MEMMAP_USABLE: u32 = 0;

// Limit wpisów mapy pamięci, które obsłużymy statycznie na stosie.
// Heap jeszcze nie istnieje w tym momencie, więc Vec jest niedostępny.
// W praktyce Limine/QEMU/realny hardware zwraca zwykle kilkanaście-kilkadziesiąt
// wpisów; 128 to bezpieczny zapas. Jeśli kiedyś to nie wystarczy, zobaczysz
// log ostrzegawczy poniżej, a nie tichy błąd.
const MAX_MEMMAP_ENTRIES: usize = 128;

unsafe extern "C" {
    fn mm_init(
        entries: *const PmmMemmapEntry,
        entry_count: u64,
        hhdm_offset: u64,
        kernel_phys_base: u64,
        kernel_virt_base: u64,
    );

    fn kmalloc(size: usize) -> *mut c_void;
    fn kfree(ptr: *mut c_void);
    fn kmalloc_aligned(size: usize, align: usize) -> *mut c_void;
    fn kfree_aligned(ptr: *mut c_void);

    fn pmm_total_frames() -> u64;
    fn pmm_free_frames() -> u64;
}

/// Wołane z C (mm.c: mm_log/mm_panic) — most do debug::log.
///
/// # Safety
/// `msg` musi być poprawnym, zakończonym NUL-em wskaźnikiem C-stringa
/// żyjącym przez czas trwania wywołania — spełnione przez wszystkie
/// wywołania w mm.c, które przekazują literały łańcuchowe.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rust_debug_log(msg: *const c_char) {
    if msg.is_null() {
        return;
    }
    let cstr = unsafe { core::ffi::CStr::from_ptr(msg) };
    match cstr.to_str() {
        Ok(s) => crate::debug::log(s),
        Err(_) => crate::debug::log("[mm] (nieprawidlowy UTF-8 w logu C)"),
    }
}

/// Inicjalizuje PMM + VMM + heap. Wołać raz z main.rs, po arch::init(),
/// mając już odpowiedzi z Limine (HHDM, memory map, adres kernela).
///
/// # Panics
/// Panikuje jeśli liczba wpisów memory mapy przekracza `MAX_MEMMAP_ENTRIES`
/// (patrz komentarz przy tej stałej) — lepiej głośno wybuchnąć niż po cichu
/// obciąć część mapy pamięci i mieć trudny do zdiagnozowania błąd PMM.
pub fn init(
    memmap_entries: &[crate::arch::x86_64::boot::MemmapEntry],
    hhdm_offset: u64,
    kernel_phys_base: u64,
    kernel_virt_base: u64,
) {
    assert!(
        memmap_entries.len() <= MAX_MEMMAP_ENTRIES,
        "mm::init: za duzo wpisow memory mapy ({}), zwieksz MAX_MEMMAP_ENTRIES",
        memmap_entries.len()
    );

    let mut buf = [PmmMemmapEntry { base: 0, length: 0, r#type: 0 }; MAX_MEMMAP_ENTRIES];
    for (i, e) in memmap_entries.iter().enumerate() {
        buf[i] = PmmMemmapEntry {
            base: e.base,
            length: e.length,
            r#type: e.entry_type,
        };
    }

    unsafe {
        mm_init(
            buf.as_ptr(),
            memmap_entries.len() as u64,
            hhdm_offset,
            kernel_phys_base,
            kernel_virt_base,
        );
    }
}

pub fn total_frames() -> u64 {
    unsafe { pmm_total_frames() }
}

pub fn free_frames() -> u64 {
    unsafe { pmm_free_frames() }
}

struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let ptr = if layout.align() > 16 {
            unsafe { kmalloc_aligned(layout.size(), layout.align()) }
        } else {
            unsafe { kmalloc(layout.size()) }
        };
        ptr as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if layout.align() > 16 {
            unsafe { kfree_aligned(ptr as *mut c_void) };
        } else {
            unsafe { kfree(ptr as *mut c_void) };
        }
    }
}

#[global_allocator]
static ALLOCATOR: KernelAllocator = KernelAllocator;

#[alloc_error_handler]
fn alloc_error_handler(layout: Layout) -> ! {
    panic!("Alokacja pamieci nie powiodla sie: {:?}", layout);
}

// `PmmMemmapEntry { .. }` w tablicy [_; MAX_MEMMAP_ENTRIES] wymaga Copy/Clone.
impl Clone for PmmMemmapEntry {
    fn clone(&self) -> Self {
        PmmMemmapEntry { base: self.base, length: self.length, r#type: self.r#type }
    }
}
impl Copy for PmmMemmapEntry {}