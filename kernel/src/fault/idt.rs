use core::arch::asm;

#[repr(C, packed)]
#[derive(Clone, Copy)]
struct IdtEntry {
    offset_low: u16,
    selector: u16,
    ist: u8,
    type_attr: u8,
    offset_mid: u16,
    offset_high: u32,
    reserved: u32,
}

#[repr(C, packed)]
struct IdtPointer {
    limit: u16,
    base: u64,
}

static mut IDT: [IdtEntry; 256] = [IdtEntry {
    offset_low: 0,
    selector: 0,
    ist: 0,
    type_attr: 0,
    offset_mid: 0,
    offset_high: 0,
    reserved: 0,
}; 256];

static mut IDT_PTR: IdtPointer = IdtPointer { limit: 0, base: 0 };

pub fn set_gate(n: usize, handler: u64, selector: u16, flags: u8) {
    unsafe {
        IDT[n].offset_low = (handler & 0xFFFF) as u16;
        IDT[n].offset_mid = ((handler >> 16) & 0xFFFF) as u16;
        IDT[n].offset_high = ((handler >> 32) & 0xFFFFFFFF) as u32;
        IDT[n].selector = selector;
        IDT[n].ist = 0;
        IDT[n].type_attr = flags;
    }
}

pub fn init() {
    unsafe {
        IDT_PTR.limit = (core::mem::size_of::<[IdtEntry; 256]>() - 1) as u16;
        IDT_PTR.base = IDT.as_ptr() as u64;
        
        asm!("lidt [{}]", in(reg) &IDT_PTR, options(nostack, preserves_flags));
    }
    crate::debug::log("IDT loaded");
}