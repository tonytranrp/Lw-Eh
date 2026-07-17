#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// Real coverage for signal<Event,N>::attach()/detach() (research.md Part A).

namespace {

struct dummy_event {
    int value;
};

int free_hits = 0;
void on_dummy_free(const dummy_event& e) {
    free_hits += e.value;
}

struct listener {
    int hits = 0;
    void on_dummy(const dummy_event& e) {
        hits += e.value;
    }
};

} // namespace

int main() {
    // Attach a free function and a member function; both fire.
    {
        free_hits = 0;
        listener l;
        lweh::signal<dummy_event> s;

        LWEH_EXPECT(s.attach<&on_dummy_free>());
        LWEH_EXPECT((s.attach<&listener::on_dummy>(&l)));

        s.publish(dummy_event{5});
        LWEH_EXPECT_EQ(free_hits, 5);
        LWEH_EXPECT_EQ(l.hits, 5);
    }

    // Detaching one listener stops it from firing; the other is unaffected.
    {
        free_hits = 0;
        listener l;
        lweh::signal<dummy_event> s;
        s.attach<&on_dummy_free>();
        s.attach<&listener::on_dummy>(&l);

        LWEH_EXPECT(s.detach<&on_dummy_free>());
        s.publish(dummy_event{3});
        LWEH_EXPECT_EQ(free_hits, 0); // detached, must not fire
        LWEH_EXPECT_EQ(l.hits, 3);    // still attached, fires normally
    }

    // Detaching something not currently attached is a no-op that reports failure.
    {
        lweh::signal<dummy_event> s;
        LWEH_EXPECT(!s.detach<&on_dummy_free>());
    }

    // Detach-then-detach-again: second call reports failure, first succeeded.
    {
        listener l;
        lweh::signal<dummy_event> s;
        s.attach<&listener::on_dummy>(&l);
        LWEH_EXPECT(s.detach<&listener::on_dummy>(&l));
        LWEH_EXPECT(!s.detach<&listener::on_dummy>(&l));
    }

    return lweh_test::finish();
}
