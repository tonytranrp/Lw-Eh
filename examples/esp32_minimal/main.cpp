// Minimal freestanding smoke example: NOT flashable production firmware.
// Exercises a representative slice of the public API for size measurement —
// reused verbatim by size_audit/ (Research/ARCHITECTURE.md: "one source of
// truth for realistic usage").
//
// TODO(Phase 1+): replace the commented-out calls below with real
// attach()/publish() once signal<> is implemented.

#include <lweh/lweh.hpp>

struct button_event {
    unsigned pin;
    bool long_press;
};

static lweh::signal<button_event, 4> button_signal;

extern "C" void app_main() {
    // TODO(Phase 1+): button_signal.attach<...>(...); button_signal.publish(...);
    (void)button_signal;
}
