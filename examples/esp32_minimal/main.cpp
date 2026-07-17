// Minimal freestanding smoke example: NOT flashable production firmware.
// The actual usage logic lives in ../scenario.hpp, shared verbatim with the
// other three architectures' examples and size_audit/'s host-proxy probe so
// the example and the size number can never silently drift apart
// (Research/ARCHITECTURE.md: "one source of truth for realistic usage").

#include "../scenario.hpp"
#include "uart.hpp"

extern "C" void app_main() {
    lweh_example::uart0_init_baud_defensive();
    lweh_example::uart0_tx_string("LWEH BOOT\r\n");
    lweh_example::run_scenario(2, 42, true, 50, -60, true);
    lweh_example::uart0_tx_string("LWEH OK\r\n");
}
