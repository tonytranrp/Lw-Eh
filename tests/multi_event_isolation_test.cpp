#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// TODO(Phase 1): once signal<Event,N>::attach()/publish() exist, replace
// this placeholder with real coverage: two distinct signal<EventA> and
// signal<EventB> instances (or two signal<Event> instances of the same
// type) never cross-trigger each other's listeners — publishing one never
// invokes listeners attached only to the other (research.md §A2: each event
// type is its own compile-time-bound signal, no shared runtime registry).

struct event_a {
    int value;
};
struct event_b {
    int value;
};

int main() {
    lweh::signal<event_a> a;
    lweh::signal<event_b> b;
    (void)a;
    (void)b;
    LWEH_EXPECT(true);
    return lweh_test::finish();
}
