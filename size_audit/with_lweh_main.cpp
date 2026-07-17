// Host-representative "with Lw-Eh" size probe -- reuses the exact same
// usage scenario as examples/esp32_minimal/main.cpp via scenario.hpp
// (research.md §B9's "host-representative... fast, always-available proxy
// signal" flavor; Research/ARCHITECTURE.md's "one source of truth for
// realistic usage"). Volatile-sourced inputs are threaded through from here
// specifically because this is the size-measurement consumer of the shared
// scenario -- confirmed necessary to get a non-zero, meaningful delta under
// the aggressive flag set (Research/PROGRESS.md, firings 2-3).

#include "../examples/esp32_minimal/scenario.hpp"

volatile unsigned g_pin_input = 2;
volatile int g_sensor_input = 42;

int main() {
    lweh_example::run_scenario(g_pin_input, g_sensor_input);
    return (lweh_example::g_logger.last_value == g_sensor_input) ? 0 : 1;
}
