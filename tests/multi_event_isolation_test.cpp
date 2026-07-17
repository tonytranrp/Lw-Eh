#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// Real coverage confirming distinct signal instances never cross-trigger
// each other's listeners — no shared runtime registry exists to leak
// through (research.md §A2/§A10; each event type is its own compile-time-
// bound signal).

namespace {

struct event_a {
    int value;
};
struct event_b {
    int value;
};

int a_hits = 0;
int b_hits = 0;
void on_a(const event_a& e) {
    a_hits += e.value;
}
void on_b(const event_b& e) {
    b_hits += e.value;
}

struct counter {
    int hits = 0;
    void on(const event_a& e) {
        hits += e.value;
    }
};

} // namespace

int main() {
    // Two different event types: publishing one never touches the other's listeners.
    {
        a_hits = b_hits = 0;
        lweh::signal<event_a> a;
        lweh::signal<event_b> b;
        a.attach<&on_a>();
        b.attach<&on_b>();

        a.publish(event_a{7});
        LWEH_EXPECT_EQ(a_hits, 7);
        LWEH_EXPECT_EQ(b_hits, 0);

        b.publish(event_b{11});
        LWEH_EXPECT_EQ(a_hits, 7);
        LWEH_EXPECT_EQ(b_hits, 11);
    }

    // Two independent instances of the SAME event type are also isolated
    // from each other -- there's no shared state behind the scenes.
    {
        counter c1;
        counter c2;
        lweh::signal<event_a> s1;
        lweh::signal<event_a> s2;
        s1.attach<&counter::on>(&c1);
        s2.attach<&counter::on>(&c2);

        s1.publish(event_a{4});
        LWEH_EXPECT_EQ(c1.hits, 4);
        LWEH_EXPECT_EQ(c2.hits, 0);
    }

    return lweh_test::finish();
}
