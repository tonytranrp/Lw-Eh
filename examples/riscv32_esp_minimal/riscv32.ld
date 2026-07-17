/* Placeholder minimal linker script (research.md Part B toolchain notes).
   Sufficient to link a non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real memory map once cross-compilation is validated.

   Two lessons worth checking FIRST when that happens, learned the hard way
   on examples/esp32_minimal (Xtensa) but likely transferable here since
   this target is the SAME Espressif chip family (esp32c3/c6, just the
   RISC-V core), flashed with the same esptool.py:
   1. Do NOT design this script around a linker-script `AT>` LMA-vs-VMA
      split for .data (the textbook "load in flash, copy to RAM at boot"
      pattern) -- confirmed empirically that esptool's elf2image builds
      image segments from each section's VMA alone and does not implement
      `AT>` at all, so nothing ever gets written to the intended "load
      address," silently corrupting .data if a runtime copy loop trusts it
      (Research/PROGRESS.md, firings 38-46). Plain per-section placement,
      no copy loop, matches what elf2image actually does.
   2. If real hardware bring-up here also reuses a prebuilt Espressif
      2nd-stage bootloader (the way esp32_minimal reuses
      framework-arduinoespressif32's bootloader_*.elf), check whether it
      arms a boot-time watchdog that needs disabling early in app code --
      esp32_minimal's own from-scratch app had to do this explicitly
      (examples/esp32_minimal/rtc_wdt.hpp) since nothing else was going to.
   Not applicable here, confirmed Xtensa-specific and not worth re-checking
   for this target: IRAM0's word-only-access restriction (Xtensa-only
   instruction-bus quirk) and the call0-into-windowed-ABI return mismatch
   (RISC-V has no windowed register file at all). */
ENTRY(_start)
SECTIONS
{
    . = 0x42000000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
