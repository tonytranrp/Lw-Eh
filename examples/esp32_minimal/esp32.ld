/* Placeholder minimal linker script for the freestanding esp32_minimal smoke
   example (research.md Part B toolchain notes). Sufficient to link a
   non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real ESP32 memory map once flashable production firmware
   is actually needed (see Research/PROGRESS.md).

   .literal* sections MUST come before .text* in the same output section:
   Xtensa's l32r instruction only has a backward (negative) PC-relative
   offset, so a function's literal pool has to be placed at a lower address
   than the code referencing it. Confirmed the hard way -- omitting this
   (an earlier version of this file had .text/.rodata/.data/.bss and nothing
   else) let the linker's orphan-section handling place .literal* wherever
   it wanted, which produced "dangerous relocation: l32r: literal placed
   after use" link failures against the real xtensa-esp32-elf-gcc 8.4.0
   toolchain the moment this was actually cross-compiled for the first time. */
ENTRY(_start)
SECTIONS
{
    . = 0x40080000;
    .text : { *(.literal*) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
