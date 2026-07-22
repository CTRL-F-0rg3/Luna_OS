pub mod boot;
pub mod serial;

pub fn init() {
    serial::init();
    boot::parse_info();
}