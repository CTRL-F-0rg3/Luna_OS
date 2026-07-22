OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS {
    requests  PT_LOAD    FLAGS((1 << 1) | (1 << 2)); /* R + W, skanowane przez Limine */
    text      PT_LOAD    FLAGS((1 << 0) | (1 << 2)); /* R + X */
    rodata    PT_LOAD    FLAGS((1 << 2));             /* R only */
    data      PT_LOAD    FLAGS((1 << 1) | (1 << 2));  /* R + W */
}

SECTIONS {
    /* Higher-half: wymagane przez protokol Limine */
    . = 0xffffffff80000000;

    _kernel_start = .;

    .requests_start_marker : {
        KEEP(*(.requests_start_marker))
    } :requests

    .requests : {
        KEEP(*(.requests))
    } :requests

    .requests_end_marker : {
        KEEP(*(.requests_end_marker))
    } :requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    _kernel_text_start = .;
    .text : {
        *(.text .text.*)
    } :text
    _kernel_text_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    _kernel_rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    _kernel_rodata_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    _kernel_data_start = .;
    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    _kernel_data_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
