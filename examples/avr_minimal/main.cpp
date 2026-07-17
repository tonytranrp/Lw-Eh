// Minimal smoke example for AVR: avr-libc supplies crt0/startup, so a plain
// main() is the entry point here — unlike the other examples/*_minimal/,
// which are fully freestanding and need explicit startup stubs.
// TODO(Phase 1+): replace the commented-out calls below with real
// attach()/publish() once signal<> is implemented.

#include <lweh/lweh.hpp>

struct button_event {
    unsigned pin;
    bool long_press;
};

static lweh::signal<button_event, 4> button_signal;

int main() {
    // TODO(Phase 1+): button_signal.attach<...>(...); button_signal.publish(...);
    (void)button_signal;
    for (;;) {
    }
}
