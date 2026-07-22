use core::panic::PanicInfo;
use crate::debug;

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    debug::set_panic_mode();
    
    debug::log("╔══════════════════════════════════════════════════════════════╗");
    debug::log("║                    LUNA OS KERNEL PANIC                      ║");
    debug::log("╠══════════════════════════════════════════════════════════════╣");
    
    debug::log_fmt(format_args!("║ Message:  {}", info.message()));
    
    if let Some(loc) = info.location() {
        debug::log_fmt(format_args!("║ File:     {}", loc.file()));
        debug::log_fmt(format_args!("║ Line:     {}", loc.line()));
        debug::log_fmt(format_args!("║ Column:   {}", loc.column()));
    }
    
    debug::log("╚══════════════════════════════════════════════════════════════╝");
    debug::log("System halted. Reset required.");
    
    loop {
        unsafe { core::arch::asm!("hlt", options(nomem, nostack)); }
    }
}