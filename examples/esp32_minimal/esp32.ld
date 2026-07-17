/* Placeholder minimal linker script for the freestanding esp32_minimal smoke
   example (research.md Part B toolchain notes). Sufficient to link a
   non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real ESP32 memory map once cross-compilation is validated
   on a machine with the Xtensa toolchain installed (see
   Research/PROGRESS.md environment note). */
ENTRY(_start)
SECTIONS
{
    . = 0x40080000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
