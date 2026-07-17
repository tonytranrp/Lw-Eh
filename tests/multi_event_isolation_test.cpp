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

// A single listener multiply-inheriting intrusive_node<event_a> and
// intrusive_node<event_b> -- a realistic pattern (one object caring about
// several event types) never previously tested for intrusive_signal<>.
// intrusive_node<Event> privately inherits detail::intrusive_link
// non-virtually, and event_a/event_b are unrelated template instantiations,
// so this produces two genuinely distinct intrusive_link subobjects (two
// independent `next` pointers at different offsets) -- not a diamond, no
// aliasing. Confirmed correct by construction via a throwaway repro before
// adding this as a real test.
struct dual_listener : lweh::intrusive_node<event_a>, lweh::intrusive_node<event_b> {
    int a_hits = 0;
    int b_hits = 0;
    void on_event(const event_a& e) override { a_hits += e.value; }
    void on_event(const event_b& e) override { b_hits += e.value; }
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

    // Same two checks as above, for intrusive_signal<>: distinct signal
    // instances (of different AND the same event type) never cross-trigger,
    // and additionally -- a case signal<> can't even express, since it has
    // no listener base class to multiply-inherit -- one listener object
    // handling two different event types via two intrusive_node<Event>
    // subobjects stays correctly isolated between them.
    {
        lweh::intrusive_signal<event_a> sig_a;
        lweh::intrusive_signal<event_b> sig_b;
        dual_listener dl;

        sig_a.attach(dl);
        sig_b.attach(dl);

        sig_a.publish(event_a{10});
        LWEH_EXPECT_EQ(dl.a_hits, 10);
        LWEH_EXPECT_EQ(dl.b_hits, 0);

        sig_b.publish(event_b{5});
        LWEH_EXPECT_EQ(dl.a_hits, 10);
        LWEH_EXPECT_EQ(dl.b_hits, 5);

        // Detaching from one event type's signal leaves the other subobject's
        // linkage completely unaffected.
        LWEH_EXPECT(sig_a.detach(dl));
        sig_a.publish(event_a{100});
        sig_b.publish(event_b{1});
        LWEH_EXPECT_EQ(dl.a_hits, 10); // unchanged: detached from sig_a
        LWEH_EXPECT_EQ(dl.b_hits, 6);  // still attached to sig_b
    }

    return lweh_test::finish();
}
