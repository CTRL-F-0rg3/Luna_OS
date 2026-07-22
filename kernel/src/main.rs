#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

extern crate alloc;

mod arch;
mod panic;
mod debug;
mod fault;
mod mm;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    debug::init();
    debug::log("Luna OS kernel loaded");

    arch::init();
    fault::init();

    let boot_data = arch::x86_64::boot::parse_info();
    mm::init(
        &boot_data.memmap[..boot_data.memmap_len],
        boot_data.hhdm_offset,
        boot_data.kernel_phys_base,
        boot_data.kernel_virt_base,
    );

    loop {
        unsafe { core::arch::asm!("hlt", options(nomem, nostack)); }
    }
}