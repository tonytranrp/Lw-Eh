#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// TODO(Phase 1): once signal<Event,N>::publish() exists, replace this
// placeholder with real coverage: attach several listeners, publish once,
// assert they fire in the documented order, and specifically confirm a
// listener that detaches itself mid-dispatch doesn't skip or double-call the
// next listener (research.md §A9's EnTT-reverse-iteration lesson).

struct dummy_event {
    int value;
};

int main() {
    lweh::signal<dummy_event> s;
    (void)s;
    LWEH_EXPECT(true);
    return lweh_test::finish();
}
