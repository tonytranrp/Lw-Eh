/* Placeholder minimal linker script (research.md Part B toolchain notes).
   Sufficient to link a non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real memory map once cross-compilation is validated. */
ENTRY(_start)
SECTIONS
{
    . = 0x42000000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
