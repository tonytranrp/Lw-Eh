/* Placeholder minimal linker script (research.md Part B toolchain notes).
   Sufficient to link a non-bootable ELF for header/ABI/size validation only.
   TODO(Phase 2+): real memory map for a specific Cortex-M part once
   cross-compilation is validated.

   Cross-reference against the four real, hardware-only bugs the ESP32/Xtensa
   example needed (see Research/PROGRESS.md and riscv32.ld's sibling note) --
   confirmed via ARM architecture docs, not assumed, since no arm-none-eabi-gcc
   is installed on this machine to check empirically:
     - IRAM0's word-only-access restriction does NOT transfer: Cortex-M's Code
       region is ordinary byte-accessible memory on the same bus as data (the
       ubiquitous flash-resident string-literal case depends on this). Two
       real but narrower/unrelated caveats exist instead: Cortex-M0/M0+ have
       no unaligned-access support at all (M3/M4/M7 do, for most load/store
       forms), and bit-banding (M3/M4/M7 only) is an address-translation
       trick over specific SRAM/peripheral windows, never Code -- misusing it
       is a software addressing bug, not a hardware width fault like IRAM0.
     - The AT>/LMA-vs-VMA copy-down pattern is *not* at risk here the way it
       was under esptool.py: arm-none-eabi-objcopy -O binary and the standard
       flashing tools (OpenOCD, ST-Link, J-Link, pyOCD) all place sections at
       their LMA correctly, so AT> is normal, well-supported Cortex-M
       practice -- the esptool.py gap was tool-specific to Espressif, not a
       general embedded-linker hazard.
     - The call0-into-windowed-ABI mismatch is categorically inapplicable:
       Cortex-M has a flat R0-R15 register file and no windowed-register
       concept at all, so there's no analogous transition to get wrong.
   A real gap this file DOES have, more fundamental than any of the three
   above: ENTRY(Reset_Handler) below only sets the ELF's e_entry header field
   for debugger/simulator bookkeeping -- real silicon never reads it. Cortex-M
   boots by the CPU loading word @ address 0 straight into MSP and word @
   address 4 straight into PC; startup.c currently defines no vector-table
   array at all, so there's nothing correctly placed there yet. Before this
   can run on real hardware it needs a real `.isr_vector` section (initial
   MSP value, Reset_Handler address, and at minimum the fault/NMI handlers so
   a stray trap doesn't vector into garbage), `KEEP()`-ed at the base address
   so --gc-sections can't discard it (nothing else references it by name). */
ENTRY(Reset_Handler)
SECTIONS
{
    . = 0x08000000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
