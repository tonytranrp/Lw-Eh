/* Placeholder minimal linker script (research.md Part B toolchain notes).
   Sufficient to link a non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real memory map for a specific Cortex-M part once
   cross-compilation is validated. */
ENTRY(Reset_Handler)
SECTIONS
{
    . = 0x08000000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
