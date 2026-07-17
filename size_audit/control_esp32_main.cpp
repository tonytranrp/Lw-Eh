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
//
// Calls the identical uart.hpp boot-confirmation logging and rtc_wdt.hpp
// watchdog-disable that examples/esp32_minimal/main.cpp now does
// (Research/PROGRESS.md firings 39 and 46) -- that code's cost must land
// on both sides of the diff equally, or it would silently inflate
// size_report.py's "incremental" number with cost that isn't Lw-Eh's.
//
// main.cpp's own detach() exercise (Research/PROGRESS.md, the codegen-
// sweep follow-on) is a genuinely different case: calling signal<>::
// detach()/intrusive_signal<>::detach() for the first time is REAL Lw-Eh
// logic getting exercised, and its machine-code cost legitimately belongs
// in the with-lweh side's own incremental figure -- this control binary
// correctly has none of that code, by design, and must not gain any. But
// the DIAGNOSTIC-LOGGING part of that addition (the extra uart0_tx_string
// call and its two string-literal operands) is scaffolding cost, same
// category as the BOOT/OK strings above, and needs mirroring for the same
// reason: main.cpp's ternary between "LWEH DETACH OK\r\n"/"LWEH DETACH
// FAIL\r\n" forces the compiler to allocate storage for BOTH literals
// regardless of which one is ever printed at runtime, so this side must
// pay for both too, via a value this binary can observe without needing
// any lweh code at all (a local volatile bool, not a detach() result).
//
// Same reasoning applies to main.cpp's capacity-boundary exercise (the
// four attach() calls plus the "LWEH CAPACITY OK\r\n"/"LWEH CAPACITY
// FAIL\r\n" ternary): the attach() calls themselves are real Lw-Eh logic
// and stay out of this binary; the two new string-literal operands are
// scaffolding cost mirrored the same way as above.

#include "../examples/esp32_minimal/uart.hpp"
#include "../examples/esp32_minimal/rtc_wdt.hpp"

extern "C" void app_main() {
    lweh_example::rtc_wdt_disable();
    lweh_example::uart0_init_baud_defensive();
    lweh_example::uart0_tx_string("LWEH BOOT\r\n");
    lweh_example::uart0_tx_string("LWEH OK\r\n");

    volatile bool detach_diagnostic_probe = true;
    lweh_example::uart0_tx_string(detach_diagnostic_probe ? "LWEH DETACH OK\r\n" : "LWEH DETACH FAIL\r\n");

    volatile bool capacity_diagnostic_probe = true;
    lweh_example::uart0_tx_string(capacity_diagnostic_probe ? "LWEH CAPACITY OK\r\n" : "LWEH CAPACITY FAIL\r\n");
}
