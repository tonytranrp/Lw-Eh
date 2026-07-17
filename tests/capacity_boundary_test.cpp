#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// Real coverage for signal<Event,N>'s capacity boundary at N and N+1
// listeners (research.md §A7 static-array capacity model).

namespace {

struct dummy_event {
    int value;
};

int hits_a = 0;
int hits_b = 0;
int hits_c = 0;
void hook_a(const dummy_event& e) {
    hits_a += e.value;
}
void hook_b(const dummy_event& e) {
    hits_b += e.value;
}
void hook_c(const dummy_event& e) {
    hits_c += e.value;
}

} // namespace

int main() {
    hits_a = hits_b = hits_c = 0;
    lweh::signal<dummy_event, 2> s; // MaxListeners = 2 for a tight boundary check.

    // Exactly N (=2) attaches must succeed.
    LWEH_EXPECT(s.attach<&hook_a>());
    LWEH_EXPECT(s.attach<&hook_b>());

    // The (N+1)th attach must fail — no empty slot left.
    LWEH_EXPECT(!s.attach<&hook_c>());

    // The over-capacity attempt must not have corrupted the existing two:
    // both still fire exactly as expected, and the rejected one never fires.
    s.publish(dummy_event{10});
    LWEH_EXPECT_EQ(hits_a, 10);
    LWEH_EXPECT_EQ(hits_b, 10);
    LWEH_EXPECT_EQ(hits_c, 0);

    // Freeing a slot via detach makes room for exactly one more attach.
    LWEH_EXPECT(s.detach<&hook_a>());
    LWEH_EXPECT(s.attach<&hook_c>());
    LWEH_EXPECT(!s.attach<&hook_a>()); // full again (hook_b, hook_c occupy both slots)

    hits_a = hits_b = hits_c = 0;
    s.publish(dummy_event{1});
    LWEH_EXPECT_EQ(hits_a, 0); // detached, must not fire
    LWEH_EXPECT_EQ(hits_b, 1);
    LWEH_EXPECT_EQ(hits_c, 1);

    return lweh_test::finish();
}
