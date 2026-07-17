#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// Real coverage for signal<Event,N>::publish() dispatch order (research.md
// Part A). Listeners fire in slot order, which for a signal with no prior
// detach history is attach order (attach() first-fits from index 0).
//
// Helper hooks are file-scope (anonymous namespace), not local classes: a
// local class's members lack linkage and are not reliably usable as
// template arguments for a template<auto Fn> NTTP (research.md's own
// delegate<Sig> investigation flagged this exact class of pitfall).

namespace {

struct dummy_event {
    int value;
};

struct recorder {
    char trace[8] = {};
    int count = 0;
    void record(char tag) {
        if (count < 8) {
            trace[count++] = tag;
        }
    }
};

recorder* g_order_target = nullptr;
void hook_a(const dummy_event&) {
    g_order_target->record('A');
}
void hook_b(const dummy_event&) {
    g_order_target->record('B');
}
void hook_c(const dummy_event&) {
    g_order_target->record('C');
}

lweh::signal<dummy_event, 4>* g_self_detach_sig = nullptr;
recorder* g_self_detach_target = nullptr;
void self_detaching_hook(const dummy_event&) {
    g_self_detach_target->record('S');
    g_self_detach_sig->detach<&self_detaching_hook>();
}

} // namespace

int main() {
    // Three listeners attached in order A, B, C must fire in that order.
    {
        recorder r;
        g_order_target = &r;

        lweh::signal<dummy_event, 4> s;
        s.attach<&hook_a>();
        s.attach<&hook_b>();
        s.attach<&hook_c>();
        s.publish(dummy_event{1});

        LWEH_EXPECT_EQ(r.count, 3);
        LWEH_EXPECT_EQ(r.trace[0], 'A');
        LWEH_EXPECT_EQ(r.trace[1], 'B');
        LWEH_EXPECT_EQ(r.trace[2], 'C');
    }

    // A listener that detaches itself mid-dispatch still fires exactly once
    // this pass, and never again afterward (research.md §A9 self-detach
    // contract, verified by dedicated subagent stress-testing before
    // signal<> was implemented).
    {
        recorder r;
        lweh::signal<dummy_event, 4> s;
        g_self_detach_sig = &s;
        g_self_detach_target = &r;

        s.attach<&self_detaching_hook>();
        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 1);
        LWEH_EXPECT_EQ(r.trace[0], 'S');

        s.publish(dummy_event{1}); // second pass: must not fire again
        LWEH_EXPECT_EQ(r.count, 1);
    }

    return lweh_test::finish();
}
