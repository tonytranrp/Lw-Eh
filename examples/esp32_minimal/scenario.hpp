#pragma once
#ifndef LWEH_EXAMPLE_SCENARIO_HPP_INCLUDED
#define LWEH_EXAMPLE_SCENARIO_HPP_INCLUDED

// Shared "realistic Lw-Eh usage" scenario -- three event types exercising
// both storage policies: signal<Event,N> (button_event: free-function
// listener; sensor_reading_event: member-function listener) and
// intrusive_signal<Event> (connection_event: intrusive_node listener) --
// reused verbatim by both the embedded ESP32 entry point (main.cpp) and the
// host-proxy size probe (size_audit/with_lweh_main.cpp), so the example and
// the size number can never silently drift apart (Research/ARCHITECTURE.md:
// "one source of truth for realistic usage"). A distinct third event type is
// used for the intrusive listener, rather than adding a second storage
// policy to an existing event type, so the two policies read as genuinely
// separate, independently understandable examples.

#include <lweh/lweh.hpp>

namespace lweh_example {

struct button_event {
    unsigned pin;
    bool long_press;
};

struct sensor_reading_event {
    unsigned sensor_id;
    int value;
};

struct connection_event {
    bool connected;
};

inline lweh::signal<button_event, 4> g_button_signal;
inline lweh::signal<sensor_reading_event, 4> g_sensor_signal;
inline lweh::intrusive_signal<connection_event> g_connection_signal;
inline volatile unsigned g_led_state = 0;

inline void on_button(const button_event& e) {
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

inline sensor_logger g_logger;

class connection_logger : public lweh::intrusive_node<connection_event> {
public:
    void on_event(const connection_event& e) override {
        connected = e.connected;
    }
    bool connected = false;
};

inline connection_logger g_connection_logger;

// pin_input/sensor_input/connected_input should come from a volatile read
// at the call site (or be volatile themselves) when this is used for size
// measurement, so the aggressive flag set can't fully constant-fold the
// whole scenario away -- confirmed necessary empirically (Research/
// PROGRESS.md's delegate-vs-signal host-proxy size-probe findings, firings
// 2-3). The embedded example entry point doesn't need this care since it's
// not being used to measure anything, just to demonstrate/exercise the API.
inline void run_scenario(unsigned pin_input, int sensor_input, bool connected_input) {
    g_button_signal.attach<&on_button>();
    g_sensor_signal.attach<&sensor_logger::on_reading>(&g_logger);
    g_connection_signal.attach(g_connection_logger);

    g_button_signal.publish(button_event{pin_input, true});
    g_sensor_signal.publish(sensor_reading_event{0, sensor_input});
    g_connection_signal.publish(connection_event{connected_input});
}

} // namespace lweh_example

#endif // LWEH_EXAMPLE_SCENARIO_HPP_INCLUDED
