use core::arch::asm;

#[repr(C, packed)]
struct GdtEntry {
    limit_low: u16,
    base_low: u16,
    base_mid: u8,
    access: u8,
    granularity: u8,
    base_high: u8,
}

#[repr(C, packed)]
struct GdtPointer {
    limit: u16,
    base: u64,
}

static mut GDT: [GdtEntry; 5] = [
    GdtEntry { limit_low: 0, base_low: 0, base_mid: 0, access: 0, granularity: 0, base_high: 0 },
    GdtEntry { limit_low: 0xFFFF, base_low: 0, base_mid: 0, access: 0x9A, granularity: 0xAF, base_high: 0 },
    GdtEntry { limit_low: 0xFFFF, base_low: 0, base_mid: 0, access: 0x92, granularity: 0xCF, base_high: 0 },
    GdtEntry { limit_low: 0xFFFF, base_low: 0, base_mid: 0, access: 0xFA, granularity: 0xAF, base_high: 0 },
    GdtEntry { limit_low: 0xFFFF, base_low: 0, base_mid: 0, access: 0xF2, granularity: 0xCF, base_high: 0 },
];

static mut GDT_PTR: GdtPointer = GdtPointer { limit: 0, base: 0 };

pub fn init() {
    unsafe {
        GDT_PTR.limit = (core::mem::size_of::<[GdtEntry; 5]>() - 1) as u16;
        GDT_PTR.base = GDT.as_ptr() as u64;
        
        asm!("lgdt [{}]", in(reg) &GDT_PTR, options(nostack, preserves_flags));
        asm!(
            "push 0x08",
            "lea rax, [2f]",
            "push rax",
            "retfq",
            "2:",
            "mov ax, 0x10",
            "mov ds, ax",
            "mov es, ax",
            "mov fs, ax",
            "mov gs, ax",
            "mov ss, ax",
            out("rax") _,
            options(nostack)
        );
        
    }
    crate::debug::log("GDT loaded");
}