// Minimal freestanding smoke example. Genuinely bootable and confirmed
// running correctly on real ESP32 hardware (Research/PROGRESS.md firing
// 45: LWEH BOOT/LWEH OK both print over real UART). The actual usage
// logic lives in ../scenario.hpp, shared verbatim with the other three
// architectures' examples and size_audit/'s host-proxy probe so the
// example and the size number can never silently drift apart
// (Research/ARCHITECTURE.md: "one source of truth for realistic usage").

#include "../scenario.hpp"
#include "uart.hpp"
#include "rtc_wdt.hpp"

extern "C" void app_main() {
    lweh_example::rtc_wdt_disable();
    lweh_example::uart0_init_baud_defensive();
    lweh_example::uart0_tx_string("LWEH BOOT\r\n");
    lweh_example::run_scenario(2, 42, true, 50, -60, true);
    lweh_example::uart0_tx_string("LWEH OK\r\n");

    // Never return. app_main() is entered via startup.S's `call0` (a
    // non-windowed call) into this windowed-ABI (entry/retw) function.
    // Real hardware confirmed (firing 45) every windowed call/return
    // NESTED inside run_scenario() already works correctly -- the one
    // transition that doesn't is this function's own outermost retw.n,
    // returning back across the call0 boundary into _start, which never
    // set up Xtensa window-overflow/underflow exception vectors (real
    // ESP-IDF/Arduino startup code does this; this from-scratch _start
    // deliberately doesn't -- implementing real window-exception vectors
    // is a substantial undertaking this project has no independent need
    // for). Confirmed via a real, decoded `Fatal exception (28):
    // LoadProhibited` landing inside the bootloader's own code range
    // immediately after LWEH OK printed, consistent with retw.n reading
    // a garbage/stale return address at that one boundary. Never
    // attempting the return sidesteps it entirely -- and matches how
    // bare-metal/Arduino-style entry points are meant to behave anyway:
    // there's no OS here for app_main() to hand control back to.
    for (;;) {
    }
}
