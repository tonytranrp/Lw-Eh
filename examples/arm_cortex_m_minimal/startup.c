/* Placeholder reset handler (ARM Cortex-M). Sufficient to link only — no
   real vector table, no .data copy-down, no .bss zeroing, so this must not
   be flashed to real hardware as-is.
   TODO(Phase 2+): real vector table + .data copy-down + .bss zeroing once
   cross-compilation is validated (see Research/PROGRESS.md). */
extern void app_main(void);

void Reset_Handler(void) {
    app_main();
    for (;;) {
    }
}
