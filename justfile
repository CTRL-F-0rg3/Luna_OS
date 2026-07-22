set shell := ["bash", "-cu"]

LIMINE_DIR := "build/limine-local"
target := "x86_64-unknown-none"
kernel_elf := "kernel/target/x86_64-unknown-none/debug/luna-kernel"
iso_dir := "build/iso"
iso_file := "build/luna-os.iso"

default: build

# Pobiera gotowe binarki Limine (branch v7.x-binary, pasuje do Limine 7.0.0)
fetch-limine:
    rm -rf {{LIMINE_DIR}}
    git clone https://github.com/limine-bootloader/limine.git \
        --branch v7.x-binary --depth=1 {{LIMINE_DIR}}
    make -C {{LIMINE_DIR}}

build:
    cd kernel && cargo +nightly build --target x86_64-unknown-none

iso: build
    mkdir -p {{iso_dir}}/boot
    cp {{kernel_elf}} {{iso_dir}}/boot/kernel.elf
    cp boot/limine.cfg {{iso_dir}}/boot/
    cp {{LIMINE_DIR}}/limine-bios.sys {{iso_dir}}/boot/
    cp {{LIMINE_DIR}}/limine-bios-cd.bin {{iso_dir}}/boot/
    cp {{LIMINE_DIR}}/limine-uefi-cd.bin {{iso_dir}}/boot/
    xorriso -as mkisofs -b boot/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot boot/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image \
        --protective-msdos-label \
        {{iso_dir}} -o {{iso_file}}
    {{LIMINE_DIR}}/limine bios-install {{iso_file}}

run: iso
    qemu-system-x86_64 \
        -cdrom {{iso_file}} \
        -serial stdio \
        -m 256M \
        -no-reboot \
        -no-shutdown

clean:
    cd kernel && cargo clean
    rm -rf build/
