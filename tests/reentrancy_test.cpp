#include "test_harness.hpp"
#include <lweh/lweh.hpp>

// Real coverage for signal<Event,N>'s dispatch-safety contract under
// reentrant attach/detach during publish() (research.md §A9). These
// scenarios were empirically stress-tested (compiled, ASan/UBSan-checked)
// by a dedicated investigation before signal<> was implemented; this suite
// re-verifies the same properties against the real library, not a model of
// it. Hooks are forward-declared so any hook's body may reference any other
// regardless of definition order.

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

// --- Scenario 1: detach a not-yet-visited peer during dispatch ---
void s1_hook_a(const dummy_event&);
void s1_hook_b(const dummy_event&);
void s1_hook_c(const dummy_event&);
void s1_hook_d(const dummy_event&);

recorder* g1_rec = nullptr;
lweh::signal<dummy_event, 4>* g1_sig = nullptr;

void s1_hook_a(const dummy_event&) {
    g1_rec->record('A');
    g1_sig->detach<&s1_hook_d>(); // D hasn't fired yet this pass
}
void s1_hook_b(const dummy_event&) {
    g1_rec->record('B');
}
void s1_hook_c(const dummy_event&) {
    g1_rec->record('C');
}
void s1_hook_d(const dummy_event&) {
    g1_rec->record('D');
}

// --- Scenario 2: detach an already-visited peer during dispatch ---
void s2_hook_a(const dummy_event&);
void s2_hook_c(const dummy_event&);

recorder* g2_rec = nullptr;
lweh::signal<dummy_event, 4>* g2_sig = nullptr;

void s2_hook_a(const dummy_event&) {
    g2_rec->record('A');
}
void s2_hook_b(const dummy_event&) {
    g2_rec->record('B');
}
void s2_hook_c(const dummy_event&) {
    g2_rec->record('C');
    g2_sig->detach<&s2_hook_a>(); // A already fired earlier this same pass
}
void s2_hook_d(const dummy_event&) {
    g2_rec->record('D');
}

// --- Scenario 3: detach+attach in one callback; the freed slot is behind
// the current cursor, so the new listener is deferred to the next pass ---
void s3_hook_a(const dummy_event&);
void s3_hook_f(const dummy_event&);

recorder* g3_rec = nullptr;
lweh::signal<dummy_event, 4>* g3_sig = nullptr;

void s3_hook_a(const dummy_event&) {
    g3_rec->record('A');
}
void s3_hook_b(const dummy_event&) {
    g3_rec->record('B');
    g3_sig->detach<&s3_hook_a>();  // frees slot 0 (already visited, safe)
    g3_sig->attach<&s3_hook_f>();  // first-fit lands in slot 0, index 0 <
                                    // current cursor 1 -> deferred per contract
    g3_sig->detach<&s3_hook_b>();  // self-detach: one-shot trigger, don't
                                    // repeat this dance on the next publish()
}
void s3_hook_c(const dummy_event&) {
    g3_rec->record('C');
}
void s3_hook_f(const dummy_event&) {
    g3_rec->record('F');
}

} // namespace

int main() {
    // Scenario 1
    {
        recorder r;
        g1_rec = &r;
        lweh::signal<dummy_event, 4> s;
        g1_sig = &s;
        s.attach<&s1_hook_a>();
        s.attach<&s1_hook_b>();
        s.attach<&s1_hook_c>();
        s.attach<&s1_hook_d>();

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 3);
        LWEH_EXPECT_EQ(r.trace[0], 'A');
        LWEH_EXPECT_EQ(r.trace[1], 'B');
        LWEH_EXPECT_EQ(r.trace[2], 'C'); // D skipped: detached before its slot was reached

        s.publish(dummy_event{1}); // D must stay detached permanently, not just this pass
        LWEH_EXPECT_EQ(r.count, 6);
        LWEH_EXPECT_EQ(r.trace[3], 'A');
        LWEH_EXPECT_EQ(r.trace[4], 'B');
        LWEH_EXPECT_EQ(r.trace[5], 'C');
    }

    // Scenario 2
    {
        recorder r;
        g2_rec = &r;
        lweh::signal<dummy_event, 4> s;
        g2_sig = &s;
        s.attach<&s2_hook_a>();
        s.attach<&s2_hook_b>();
        s.attach<&s2_hook_c>();
        s.attach<&s2_hook_d>();

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 4);
        LWEH_EXPECT_EQ(r.trace[0], 'A'); // A already fired before C detaches it -- no double-invoke
        LWEH_EXPECT_EQ(r.trace[1], 'B');
        LWEH_EXPECT_EQ(r.trace[2], 'C');
        LWEH_EXPECT_EQ(r.trace[3], 'D');

        s.publish(dummy_event{1}); // A must not fire again
        LWEH_EXPECT_EQ(r.count, 7);
        LWEH_EXPECT_EQ(r.trace[4], 'B');
        LWEH_EXPECT_EQ(r.trace[5], 'C');
        LWEH_EXPECT_EQ(r.trace[6], 'D');
    }

    // Scenario 3: documented attach-during-dispatch deferral contract
    {
        recorder r;
        g3_rec = &r;
        lweh::signal<dummy_event, 4> s;
        g3_sig = &s;
        s.attach<&s3_hook_a>(); // slot 0
        s.attach<&s3_hook_b>(); // slot 1
        s.attach<&s3_hook_c>(); // slot 2

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 3);
        LWEH_EXPECT_EQ(r.trace[0], 'A');
        LWEH_EXPECT_EQ(r.trace[1], 'B');
        LWEH_EXPECT_EQ(r.trace[2], 'C'); // F attached into slot 0 (behind cursor) -- deferred

        s.publish(dummy_event{1}); // F now resident at slot 0 -- fires;
                                    // B self-detached last pass, so it's
                                    // gone; only C is otherwise unaffected.
        LWEH_EXPECT_EQ(r.count, 5);
        LWEH_EXPECT_EQ(r.trace[3], 'F');
        LWEH_EXPECT_EQ(r.trace[4], 'C');
    }

    return lweh_test::finish();
}
