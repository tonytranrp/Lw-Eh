// Minimal freestanding smoke example: NOT flashable production firmware.
// The actual usage logic lives in scenario.hpp, shared verbatim with
// size_audit/'s host-proxy probe so the example and the size number can
// never silently drift apart (Research/ARCHITECTURE.md: "one source of
// truth for realistic usage").

#include "scenario.hpp"

extern "C" void app_main() {
    lweh_example::run_scenario(2, 42, true, 50);
}
