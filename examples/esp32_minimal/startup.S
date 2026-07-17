/* Real reset-vector/_start stub for the freestanding esp32_minimal example.
   Genuinely bootable on real hardware -- see Research/PROGRESS.md firing 38
   for the investigation (assembled, compiled, and linked against the real
   toolchain and this exact example's object graph, then confirmed via
   readelf/objdump that it produces the correct layout and relocates the
   previously-faulting IRAM0 byte-store off the instruction bus) this
   sequence is based on.

   Supersedes the original placeholder ("does not zero .bss or copy .data,
   so this must not be flashed to real hardware as-is") now that real
   hardware is actually available to validate against. Does exactly what
   scenario.hpp's global object graph was confirmed (via nm on the real
   linked ELF: zero _GLOBAL__sub_I_* symbols, zero .init_array/.ctors
   content) to actually need -- no more, no less:
     - .bss zeroing: 7 of scenario.hpp's 9 globals (both intrusive_signal<>
       instances, all four signal<Event,4> instances, sensor_logger) are
       all-zero and land here; nothing more than zero-fill is required.
     - .data: the 2 intrusive_node<Event>-derived listener globals
       (connection_logger, motion_logger) land here with link-time-resolved
       vtable-pointer bytes already baked in -- no runtime copy needed or
       wanted; see esp32.ld's comment on why a load-address copy loop was
       tried here, found actively wrong on real hardware (firing 43:
       esptool's elf2image doesn't implement AT>-style LMA/VMA splits, so
       there was never anything valid at the "load address" to copy from),
       and removed. The loader places .data's real bytes directly at their
       run address; nothing in startup.S needs to touch it.
     - .ctors walk: defensive, currently always a zero-iteration no-op
       (matches the finding above) -- see esp32.ld's own comment on why
       this stays in despite doing nothing today. */

    .global _start
    .align  4
_start:
    /* Stack pointer: esp32.ld reserves no inherited-stack assumption
       (unlike the previous placeholder script, which reserved no stack
       region at all) -- set it explicitly rather than trust whatever a1
       holds on entry. */
    movi    a1, _stack_top

    /* Zero .bss */
    movi    a2, _bss_start
    movi    a3, _bss_end
    j       .Lbss_check
.Lbss_loop:
    movi.n  a4, 0
    s32i    a4, a2, 0
    addi    a2, a2, 4
.Lbss_check:
    bltu    a2, a3, .Lbss_loop

    /* .data needs no runtime copy on this boot path -- see esp32.ld's
       comment for why (esptool's elf2image places section content
       directly at its run address; there is no separate "load address"
       to copy from here the way there would be on a typical execute-
       in-place-from-flash target). */

    /* Walk .ctors (not .init_array -- this toolchain rejects
       -fuse-init-array outright, confirmed directly; see esp32.ld) */
    movi    a2, __ctors_start
    movi    a3, __ctors_end
    j       .Lctors_check
.Lctors_loop:
    l32i    a4, a2, 0
    callx0  a4
    addi    a2, a2, 4
.Lctors_check:
    bltu    a2, a3, .Lctors_loop

    call0   app_main
1:  j       1b
