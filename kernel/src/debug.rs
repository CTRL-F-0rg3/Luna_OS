use core::fmt::{self, Write};
use spin::Mutex;
use uart_16550::SerialPort;

static SERIAL: Mutex<SerialPort> = Mutex::new(unsafe { SerialPort::new(0x3F8) });
static mut PANIC_MODE: bool = false;

pub fn init() {
    let mut serial = SERIAL.lock();
    serial.init();
    drop(serial);
}

pub fn set_panic_mode() {
    unsafe { PANIC_MODE = true; }
}

pub fn log(msg: &str) {
    let mut serial = SERIAL.lock();
    let _ = serial.write_str("[LUNA] ");
    let _ = serial.write_str(msg);
    let _ = serial.write_str("\r\n");
}

pub fn log_fmt(args: fmt::Arguments) {
    let mut serial = SERIAL.lock();
    let _ = serial.write_fmt(args);
    let _ = serial.write_str("\r\n");
}