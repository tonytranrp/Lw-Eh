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

    // Exercise detach() on real hardware -- the one core API surface
    // run_scenario() never touches (it only attaches and publishes), and so
    // the one piece of Lw-Eh's logic with zero real-Xtensa-codegen evidence
    // before this (Research/PROGRESS.md's codegen deep-verification sweep
    // flagged this explicitly as an observation, not a defect: nothing about
    // detach()'s implementation is architecture-specific the way the four
    // real hardware bugs were, but it had genuinely never been compiled into
    // any ESP32 binary). One call per storage policy, matching the two real
    // detach() call shapes the library has: signal<Event,N>'s free-function
    // detach<Fn>() (array compaction under the hood) and
    // intrusive_signal<Event>'s detach-by-reference (linked-list unlink()).
    // Both must return true -- each listener was attached moments ago by
    // run_scenario() and neither has fired a self-detach, so this is the
    // ordinary, non-reentrant detach path, not the documented
    // dispatch-time-reentrancy caveats.
    const bool signal_detach_ok = lweh_example::g_button_signal.detach<&lweh_example::on_button>();
    const bool intrusive_detach_ok = lweh_example::g_connection_signal.detach(lweh_example::g_connection_logger);
    lweh_example::uart0_tx_string((signal_detach_ok && intrusive_detach_ok) ? "LWEH DETACH OK\r\n" : "LWEH DETACH FAIL\r\n");

    // Exercise the capacity-boundary path on real hardware too: signal<Event,
    // 4>::attach() must reject a 5th listener once all 4 slots are occupied,
    // rather than silently overrunning the array. run_scenario() only ever
    // attaches 1 listener per signal<> (well under capacity), so -- like
    // detach() above -- this path has never been compiled into any ESP32
    // binary before now, despite being thoroughly host-tested
    // (tests/capacity_boundary_test.cpp). g_battery_signal already holds 1
    // listener from run_scenario(); which function fills the remaining 3
    // slots doesn't matter for a pure occupancy check, so on_battery_level is
    // reused (signal<>, like intrusive_signal<>, has no double-attach guard
    // -- see signal.hpp's attach() scanning purely for an empty slot). The
    // 5th call must return false.
    lweh_example::g_battery_signal.attach<&lweh_example::on_battery_level>();
    lweh_example::g_battery_signal.attach<&lweh_example::on_battery_level>();
    lweh_example::g_battery_signal.attach<&lweh_example::on_battery_level>();
    const bool fifth_attach_rejected = !lweh_example::g_battery_signal.attach<&lweh_example::on_battery_level>();
    lweh_example::uart0_tx_string(fifth_attach_rejected ? "LWEH CAPACITY OK\r\n" : "LWEH CAPACITY FAIL\r\n");

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
