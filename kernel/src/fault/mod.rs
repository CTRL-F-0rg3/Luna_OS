pub mod gdt;
pub mod idt;
pub mod isr;
pub mod dump;

pub fn init() {
    gdt::init();
    idt::init();
    crate::debug::log("Fault handling initialized");
}