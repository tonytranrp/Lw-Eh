// Minimal smoke example for AVR: avr-libc supplies crt0/startup, so a plain
// main() is the entry point here — unlike the other examples/*_minimal/,
// which are fully freestanding and need explicit startup stubs. NOT
// flashable production firmware. The actual usage logic lives in
// ../scenario.hpp, shared verbatim with the other three architectures'
// examples and size_audit/'s host-proxy probe so the example and the size
// number can never silently drift apart (Research/ARCHITECTURE.md: "one
// source of truth for realistic usage"). Brought up to date from its
// original Phase-0 stub state in firing 20 (Research/PROGRESS.md) -- this
// file used to be a dead stub that never called attach()/publish() at all,
// left behind after signal<> was actually implemented back in firing 3
// because no avr-gcc has ever been available on this machine to catch the
// staleness by trying to build it.

#include "../scenario.hpp"

int main() {
    lweh_example::run_scenario(2, 42, true, 50, -60, true);
    for (;;) {
    }
}
