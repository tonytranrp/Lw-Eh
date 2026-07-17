// Minimal freestanding smoke example: NOT flashable production firmware.
// Exercises a representative slice of the public API for size measurement —
// reused verbatim by size_audit/ (Research/ARCHITECTURE.md: "one source of
// truth for realistic usage"). Deliberately has TWO distinct event types
// (not just one) so a real cross-compiled build of this file is what would
// give the multi-event-type size data the deferred detail::signal_core
// core-extraction decision (Research/ARCHITECTURE.md, research.md §A4/§A10)
// is waiting on — this file is the fixture for that measurement, whenever a
// real embedded toolchain is available to build it (see Research/PROGRESS.md
// environment note); in the meantime it's mirrored by a host-proxy probe.

#include <lweh/lweh.hpp>

struct button_event {
    unsigned pin;
    bool long_press;
};

struct sensor_reading_event {
    unsigned sensor_id;
    int value;
};

static lweh::signal<button_event, 4> button_signal;
static lweh::signal<sensor_reading_event, 4> sensor_signal;

static volatile unsigned g_led_state = 0;

void on_button(const button_event& e) {
    g_led_state = e.long_press ? (g_led_state | (1u << e.pin)) : (g_led_state & ~(1u << e.pin));
}

class sensor_logger {
public:
    void on_reading(const sensor_reading_event& e) {
        last_sensor_id = e.sensor_id;
        last_value = e.value;
    }
    unsigned last_sensor_id = 0;
    int last_value = 0;
};

static sensor_logger g_logger;

extern "C" void app_main() {
    button_signal.attach<&on_button>();
    sensor_signal.attach<&sensor_logger::on_reading>(&g_logger);

    button_signal.publish(button_event{2, true});
    sensor_signal.publish(sensor_reading_event{0, 42});
}
