/* Real ESP32 memory-map-aware linker script for the freestanding
   esp32_minimal example (research.md Part B toolchain notes). Genuinely
   bootable on real hardware -- see Research/PROGRESS.md firing 38 for the
   investigation (built, linked, and disassembly-verified against the real
   toolchain and this exact example) this layout is based on.

   Supersedes the original single-region 0x40080000 placeholder, which
   linked and measured fine but was never actually bootable: it placed
   .data/.bss (which includes several `bool` globals in scenario.hpp)
   inside IRAM0, the instruction bus -- word-only accessible on real ESP32
   hardware. Confirmed via disassembly that this produced a real faulting
   instruction (an s8i single-byte store targeting an IRAM0 address, e.g.
   inside on_battery_level()) that would raise a LoadStoreError exception
   on real silicon, not just a style concern.

   IRAM0 (0x40080000, 128KB) and DRAM0 (0x3FFB0000) origins are real ESP32
   SRAM bus base addresses (Espressif docs + ESP-IDF's own
   components/esp_system/ld/esp32/memory.ld.in). DRAM0's LENGTH here is a
   deliberately conservative, comfortably-sufficient placeholder for this
   tiny example (actual .data+.bss usage is under 200 bytes) -- not a
   verified exact hardware ceiling, matching how this project has always
   treated memory-map placeholders (documented as such, tightened only if
   it ever actually matters).

   .literal* sections MUST come before .text* in the same output section:
   Xtensa's l32r instruction only has a backward (negative) PC-relative
   offset, so a function's literal pool has to be placed at a lower address
   than the code referencing it. Confirmed the hard way (see the original
   version of this file's own history) -- still applies verbatim, IRAM0's
   .text section keeps this ordering. */

MEMORY
{
    IRAM0 : ORIGIN = 0x40080000, LENGTH = 0x20000
    DRAM0 : ORIGIN = 0x3FFB0000, LENGTH = 0x10000
}

ENTRY(_start)

SECTIONS
{
    .text : { *(.literal*) *(.text*) } > IRAM0

    /* .rodata lives in DRAM0, not IRAM0 -- confirmed via real hardware
       (firing 42): a LoadStoreError at excvaddr resolving into this
       section's old IRAM0 placement, faulting on a byte-load
       (l8ui) of a string literal inside uart0_tx_string's char-by-char
       read loop. Same root cause as e438462's original .bss/.data
       finding (IRAM0 is the word-only-accessible instruction bus) --
       just a byte-accessed constant this time instead of a byte-accessed
       global variable. Not everything in .rodata is byte-accessed (the
       vtables here are read via word-sized l32i and were never a
       problem in IRAM0), but string literals are, and the linker has no
       way to split .rodata by access width -- DRAM0 supports both, so
       moving the whole section there is the simple, general, always-
       correct fix rather than trying to cherry-pick which rodata
       symbols are "safe" to leave behind. */
    .rodata : { *(.rodata*) } > DRAM0

    /* Real LMA (flash/IRAM0, where the loader puts the bytes) vs. VMA
       (DRAM0, where the code actually reads/writes them) split -- startup.S
       copies _data_load_start..+len to _data_start at boot. Verified via
       readelf -l against this exact layout: LOAD segment 2 shows
       VirtAddr=0x3ffb0000 PhysAddr=<end of .rodata>, FileSiz < MemSiz,
       the textbook signature of a correct LMA!=VMA + trailing-.bss split. */
    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > DRAM0 AT> IRAM0
    _data_load_start = LOADADDR(.data);

    .bss : {
        _bss_start = .;
        *(.bss*)
        _bss_end = .;
    } > DRAM0

    /* Legacy .ctors convention, not .init_array: this toolchain
       (xtensa-esp32-elf-g++ 8.4.0) rejects -fuse-init-array outright
       ("unrecognized command line option"), confirmed directly. Currently
       always empty -- scenario.hpp's globals all resolve to constant/zero
       initialization with no dynamic-init call needed (confirmed via nm:
       zero _GLOBAL__sub_I_* symbols, zero .init_array/.ctors content in the
       real build) -- but KEEP'd and walked defensively by startup.S so a
       future global that genuinely needs a constructor call doesn't
       silently get skipped by --gc-sections or fail to run. Iteration order
       (startup.S walks front-to-back) is unverified against the classic
       back-to-front crtbegin.o convention -- moot today since there's
       nothing to iterate, but flagged for whoever adds the first real
       dynamic-init global.  */
    .ctors : {
        __ctors_start = .;
        KEEP(*(.ctors))
        __ctors_end = .;
    } > DRAM0

    /* Small fixed stack immediately after .bss in DRAM0 -- the previous
       version of this script reserved no stack at all, meaning .bss could
       silently overlap whatever memory the inherited bootloader stack
       pointer was using. 4KB is generous for this example's call depth. */
    . = ALIGN(16);
    . = . + 0x1000;
    _stack_top = .;
}
