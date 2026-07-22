use super::idt::set_gate;
use core::arch::naked_asm;

macro_rules! isr_no_err {
    ($n:expr, $name:ident) => {
        #[unsafe(naked)]
        pub extern "C" fn $name() {
            naked_asm!(
                "push 0",
                concat!("push ", $n),
                "jmp isr_common",
            );
        }
    };
}

macro_rules! isr_err {
    ($n:expr, $name:ident) => {
        #[unsafe(naked)]
        pub extern "C" fn $name() {
            naked_asm!(
                concat!("push ", $n),
                "jmp isr_common",
            );
        }
    };
}

#[repr(C)]
pub struct InterruptFrame {
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

#[unsafe(naked)]
unsafe extern "C" fn isr_common() {
    naked_asm!(
        "push rax",
        "push rbx",
        "push rcx",
        "push rdx",
        "push rsi",
        "push rdi",
        "push rbp",
        "push r8",
        "push r9",
        "push r10",
        "push r11",
        "push r12",
        "push r13",
        "push r14",
        "push r15",
        "mov rdi, rsp",
        "call fault_handler",
        "pop r15",
        "pop r14",
        "pop r13",
        "pop r12",
        "pop r11",
        "pop r10",
        "pop r9",
        "pop r8",
        "pop rbp",
        "pop rdi",
        "pop rsi",
        "pop rdx",
        "pop rcx",
        "pop rbx",
        "pop rax",
        "add rsp, 16",
        "iretq",
    );
}

extern "C" fn fault_handler(frame: *mut InterruptFrame) {
    unsafe {
        let f = &*frame;
        let err_code = *(frame as *const u64).offset(-1);
        let int_num = *(frame as *const u64).offset(-2);

        super::dump::dump(int_num, err_code, f);
    }
}

isr_no_err!(0, isr_0);
isr_no_err!(1, isr_1);
isr_no_err!(2, isr_2);
isr_no_err!(3, isr_3);
isr_no_err!(4, isr_4);
isr_no_err!(5, isr_5);
isr_no_err!(6, isr_6);
isr_no_err!(7, isr_7);
isr_err!(8, isr_8);
isr_no_err!(9, isr_9);
isr_err!(10, isr_10);
isr_err!(11, isr_11);
isr_err!(12, isr_12);
isr_err!(13, isr_13);
isr_err!(14, isr_14);
isr_no_err!(16, isr_16);
isr_err!(17, isr_17);
isr_no_err!(18, isr_18);
isr_no_err!(19, isr_19);
isr_no_err!(20, isr_20);

pub fn init() {
    set_gate(0, isr_0 as u64, 0x08, 0x8E);
    set_gate(1, isr_1 as u64, 0x08, 0x8E);
    set_gate(2, isr_2 as u64, 0x08, 0x8E);
    set_gate(3, isr_3 as u64, 0x08, 0x8E);
    set_gate(4, isr_4 as u64, 0x08, 0x8E);
    set_gate(5, isr_5 as u64, 0x08, 0x8E);
    set_gate(6, isr_6 as u64, 0x08, 0x8E);
    set_gate(7, isr_7 as u64, 0x08, 0x8E);
    set_gate(8, isr_8 as u64, 0x08, 0x8E);
    set_gate(9, isr_9 as u64, 0x08, 0x8E);
    set_gate(10, isr_10 as u64, 0x08, 0x8E);
    set_gate(11, isr_11 as u64, 0x08, 0x8E);
    set_gate(12, isr_12 as u64, 0x08, 0x8E);
    set_gate(13, isr_13 as u64, 0x08, 0x8E);
    set_gate(14, isr_14 as u64, 0x08, 0x8E);
    set_gate(16, isr_16 as u64, 0x08, 0x8E);
    set_gate(17, isr_17 as u64, 0x08, 0x8E);
    set_gate(18, isr_18 as u64, 0x08, 0x8E);
    set_gate(19, isr_19 as u64, 0x08, 0x8E);
    set_gate(20, isr_20 as u64, 0x08, 0x8E);

    crate::debug::log("ISR handlers registered");
}