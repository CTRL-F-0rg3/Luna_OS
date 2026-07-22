use limine::BaseRevision;
use limine::request::{
    BootloaderInfoRequest, ExecutableAddressRequest, HhdmRequest, MemmapRequest,
};
use limine::{RequestsEndMarker, RequestsStartMarker};

#[used]
#[unsafe(link_section = ".requests")]
static BASE_REVISION: BaseRevision = BaseRevision::new();

#[used]
#[unsafe(link_section = ".requests")]
static BOOT_INFO: BootloaderInfoRequest = BootloaderInfoRequest::new();

#[used]
#[unsafe(link_section = ".requests")]
static HHDM_REQUEST: HhdmRequest = HhdmRequest::new();

#[used]
#[unsafe(link_section = ".requests")]
static MEMMAP_REQUEST: MemmapRequest = MemmapRequest::new();

#[used]
#[unsafe(link_section = ".requests")]
static KERNEL_ADDRESS_REQUEST: ExecutableAddressRequest = ExecutableAddressRequest::new();

#[used]
#[unsafe(link_section = ".requests_start_marker")]
static _START_MARKER: RequestsStartMarker = RequestsStartMarker::new();

#[used]
#[unsafe(link_section = ".requests_end_marker")]
static _END_MARKER: RequestsEndMarker = RequestsEndMarker::new();

/// Format wpisu memory mapy niezależny od crate `limine` — to co dostaje mm::init().
/// Numery `entry_type` odpowiadają PMM_MEMMAP_* w mm/include/pmm.h.
#[derive(Clone, Copy)]
pub struct MemmapEntry {
    pub base: u64,
    pub length: u64,
    pub entry_type: u32,
}

pub const MAX_MEMMAP_ENTRIES: usize = 128;

pub struct BootData {
    pub hhdm_offset: u64,
    pub kernel_phys_base: u64,
    pub kernel_virt_base: u64,
    pub memmap: [MemmapEntry; MAX_MEMMAP_ENTRIES],
    pub memmap_len: usize,
}

/// Parsuje odpowiedzi Limine potrzebne do zainicjalizowania mm.
/// Woła się to raz, po arch::init(), przed mm::init().
pub fn parse_info() -> BootData {
    assert!(
        BASE_REVISION.is_supported(),
        "Limine base revision nie jest wsparta przez ten bootloader"
    );

    if BOOT_INFO.response().is_some() {
        crate::debug::log("Boot info received from Limine");
    }

    let hhdm_offset = HHDM_REQUEST
        .response()
        .expect("Limine nie zwrocil HHDM response")
        .offset;

    let kaddr = KERNEL_ADDRESS_REQUEST
        .response()
        .expect("Limine nie zwrocil Kernel Address response");
    let kernel_phys_base = kaddr.physical_base;
    let kernel_virt_base = kaddr.virtual_base;

    let mm_response = MEMMAP_REQUEST
        .response()
        .expect("Limine nie zwrocil Memory Map response");

    let mut memmap = [MemmapEntry { base: 0, length: 0, entry_type: 0 }; MAX_MEMMAP_ENTRIES];
    let mut len = 0usize;

    for entry in mm_response.entries().iter() {
        if len >= MAX_MEMMAP_ENTRIES {
            crate::debug::log("UWAGA: obcieto mape pamieci - zwieksz MAX_MEMMAP_ENTRIES");
            break;
        }

        memmap[len] = MemmapEntry {
            base: entry.base,
            length: entry.length,
            entry_type: entry.type_ as u32,
        };
        len += 1;
    }

    BootData {
        hhdm_offset,
        kernel_phys_base,
        kernel_virt_base,
        memmap,
        memmap_len: len,
    }
}