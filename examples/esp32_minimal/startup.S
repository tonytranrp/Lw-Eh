/* Placeholder reset-vector/_start stub for the freestanding esp32_minimal
   smoke example. Sufficient to link only — does not zero .bss or copy
   .data, so this must not be flashed to real hardware as-is.
   TODO(Phase 2+): real Xtensa reset handler + .bss zeroing once
   cross-compilation is validated (see Research/PROGRESS.md). */
    .global _start
_start:
    call0 app_main
1:  j 1b
