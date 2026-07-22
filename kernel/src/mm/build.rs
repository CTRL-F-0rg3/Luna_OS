fn main() {
    cc::Build::new()
        .files([
            "src/mm/pmm.c",
            "src/mm/vmm.c",
            "src/mm/heap.c",
            "src/mm/mm.c",
        ])
        .include("src/mm/include")
        .flag_if_supported("-ffreestanding")
        .flag_if_supported("-fno-stack-protector")
        .flag_if_supported("-fno-pic")
        .flag_if_supported("-fno-pie")
        .flag_if_supported("-mno-red-zone")
        .flag_if_supported("-mcmodel=kernel")
        .flag_if_supported("-mno-mmx")
        .flag_if_supported("-mno-sse")
        .flag_if_supported("-mno-sse2")
        .flag_if_supported("-std=c11")
        .flag_if_supported("-Wall")
        .flag_if_supported("-Wextra")
        .compile("luna_mm");

    println!("cargo:rerun-if-changed=src/mm");
}
