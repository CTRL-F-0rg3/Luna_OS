use super::isr::InterruptFrame;
use crate::debug;

static FAULT_NAMES: [&str; 32] = [
    "Divide-by-zero",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved",
];

pub fn dump(int_num: u64, err_code: u64, frame: &InterruptFrame) {
    let name = if int_num < 32 {
        FAULT_NAMES[int_num as usize]
    } else {
        "Unknown"
    };
    
    debug::set_panic_mode();
    
    debug::log("╔══════════════════════════════════════════════════════════════╗");
    debug::log_fmt(format_args!("║  CPU EXCEPTION #{}: {}", int_num, name));
    debug::log("╠══════════════════════════════════════════════════════════════╣");
    
    if int_num == 8 || int_num == 10 || int_num == 11 || 
       int_num == 12 || int_num == 13 || int_num == 14 || int_num == 17 {
        debug::log_fmt(format_args!("║  Error Code: 0x{:016X}", err_code));
    }
    
    if int_num == 14 {
        let cr2: u64;
        unsafe { core::arch::asm!("mov {}, cr2", out(reg) cr2); }
        debug::log_fmt(format_args!("║  CR2 (fault addr): 0x{:016X}", cr2));
        debug::log_fmt(format_args!("║  Caused by: {}", page_fault_reason(err_code)));
    }
    
    debug::log("╠══════════════════════════════════════════════════════════════╣");
    debug::log_fmt(format_args!("║  RIP: 0x{:016X}  CS:  0x{:04X}", frame.rip, frame.cs));
    debug::log_fmt(format_args!("║  RSP: 0x{:016X}  SS:  0x{:04X}", frame.rsp, frame.ss));
    debug::log_fmt(format_args!("║  RFLAGS: 0x{:016X}", frame.rflags));
    debug::log("╚══════════════════════════════════════════════════════════════╝");
    debug::log("System halted.");
    
    loop {
        unsafe { core::arch::asm!("hlt", options(nomem, nostack)); }
    }
}

fn page_fault_reason(err_code: u64) -> &'static str {
    match err_code & 0x7 {
        0b000 => "supervisor read, page not present",
        0b001 => "supervisor read, protection violation",
        0b010 => "supervisor write, page not present",
        0b011 => "supervisor write, protection violation",
        0b100 => "user read, page not present",
        0b101 => "user read, protection violation",
        0b110 => "user write, page not present",
        0b111 => "user write, protection violation",
        _ => "unknown",
    }
}