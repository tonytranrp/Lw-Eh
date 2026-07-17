/* Placeholder reset stub (RISC-V). Sufficient to link only — does not zero
   .bss or copy .data, so this must not be flashed to real hardware as-is.
   TODO(Phase 2+): real reset handler + .bss zeroing once cross-compilation
   is validated (see Research/PROGRESS.md). */
    .global _start
_start:
    call app_main
1:  j 1b
