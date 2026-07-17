// ESP32-specific control baseline -- reuses examples/esp32_minimal's own
// startup.S/esp32.ld (real IRAM0/DRAM0 split, .bss zeroing, .data
// copy-down) so this binary is "otherwise identical" to
// lw_eh_example_esp32_minimal except for never including any lweh header,
// per research.md B9's stated methodology ("Build the same firmware with
// Lw-Eh's headers included... the diff is Lw-Eh's true incremental
// footprint"). control_main.cpp (the plain-main, default-crt0 baseline)
// stopped being that "same firmware" the moment esp32_minimal got its own
// custom boot path (Research/PROGRESS.md firing 38/39) -- this target
// restores the isolation the with-lweh/control diff depends on, for the
// real embedded measurement specifically. control_main.cpp is unaffected
// and remains correct as-is for the host-representative proxy pairing
// (lw_eh_size_audit_with_lweh_host), which has no custom boot path either.

extern "C" void app_main() {}
