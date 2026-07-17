#include "test_harness.hpp"
#include <lweh/intrusive_signal.hpp>

// Real coverage for intrusive_signal<Event>/intrusive_node<Event>
// (research.md §A7 alternate storage policy). Unlike signal<Event,N>'s
// bind<Fn>()/attach<Fn>() (which needs template<auto Fn> NTTP arguments and
// therefore file-scope, non-local helper functions -- see the NTTP-linkage
// lesson learned two firings ago), intrusive_signal::attach()/detach() take
// plain runtime references, so local classes inside main() are safe to use
// here: there's no NTTP involved at all.

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

struct basic_node : lweh::intrusive_node<dummy_event> {
    char tag = 0;
    recorder* rec = nullptr;
    int hits = 0;
    void on_event(const dummy_event& e) override {
        hits += e.value;
        if (rec) {
            rec->record(tag);
        }
    }
};

struct self_detach_node : lweh::intrusive_node<dummy_event> {
    recorder* rec = nullptr;
    lweh::intrusive_signal<dummy_event>* sig = nullptr;
    void on_event(const dummy_event&) override {
        rec->record('S');
        sig->detach(*this);
    }
};

struct detach_other_node : lweh::intrusive_node<dummy_event> {
    recorder* rec = nullptr;
    lweh::intrusive_signal<dummy_event>* sig = nullptr;
    lweh::intrusive_node<dummy_event>* target = nullptr;
    char tag = 0;
    void on_event(const dummy_event&) override {
        rec->record(tag);
        sig->detach(*target);
    }
};

} // namespace

int main() {
    // 1. Basic attach + publish: LIFO order (most recently attached fires first).
    {
        recorder r;
        basic_node a;
        basic_node b;
        basic_node c;
        a.tag = 'A';
        a.rec = &r;
        b.tag = 'B';
        b.rec = &r;
        c.tag = 'C';
        c.rec = &r;

        lweh::intrusive_signal<dummy_event> s;
        s.attach(a);
        s.attach(b);
        s.attach(c);
        s.publish(dummy_event{1});

        LWEH_EXPECT_EQ(r.count, 3);
        LWEH_EXPECT_EQ(r.trace[0], 'C'); // most recently attached fires first
        LWEH_EXPECT_EQ(r.trace[1], 'B');
        LWEH_EXPECT_EQ(r.trace[2], 'A');
        LWEH_EXPECT_EQ(a.hits, 1);
        LWEH_EXPECT_EQ(b.hits, 1);
        LWEH_EXPECT_EQ(c.hits, 1);
    }

    // 2. Detach stops a listener from firing; detaching an unattached node
    // is a no-op that reports failure.
    {
        recorder r;
        basic_node a;
        basic_node b;
        a.tag = 'A';
        a.rec = &r;
        b.tag = 'B';
        b.rec = &r;

        lweh::intrusive_signal<dummy_event> s;
        s.attach(a);
        s.attach(b);
        LWEH_EXPECT(s.detach(a));

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 1);
        LWEH_EXPECT_EQ(r.trace[0], 'B');
        LWEH_EXPECT_EQ(a.hits, 0);

        LWEH_EXPECT(!s.detach(a)); // already detached
    }

    // 3. Self-detach during dispatch: fires once, stays permanently gone.
    {
        recorder r;
        self_detach_node sd;
        sd.rec = &r;

        lweh::intrusive_signal<dummy_event> s;
        sd.sig = &s;
        s.attach(sd);

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 1);
        LWEH_EXPECT_EQ(r.trace[0], 'S');

        s.publish(dummy_event{1}); // must not fire again
        LWEH_EXPECT_EQ(r.count, 1);
    }

    // 4. Detach a not-yet-visited peer during dispatch: safe, correctly skipped.
    {
        recorder r;
        basic_node d_node;
        d_node.tag = 'D';
        d_node.rec = &r;
        basic_node c_node;
        c_node.tag = 'C';
        c_node.rec = &r;
        detach_other_node a_node;
        a_node.tag = 'A';
        a_node.rec = &r;
        a_node.target = &d_node;

        lweh::intrusive_signal<dummy_event> s;
        a_node.sig = &s;
        s.attach(d_node); // attach order: D, C, A -> dispatch order A, C, D (LIFO)
        s.attach(c_node);
        s.attach(a_node);

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 2); // A and C fire; D detached before its turn
        LWEH_EXPECT_EQ(r.trace[0], 'A');
        LWEH_EXPECT_EQ(r.trace[1], 'C');
    }

    // 5. Attach-during-dispatch is ALWAYS deferred to the next pass,
    // unconditionally -- unlike signal<Event,N>'s slot-index-dependent
    // contract, this one doesn't depend on timing within the pass
    // (detail::intrusive_core's doc comment; verified by adversarial
    // stress-testing before this was written).
    {
        recorder r;
        basic_node new_node;
        new_node.tag = 'N';
        new_node.rec = &r;

        struct attacher_node : lweh::intrusive_node<dummy_event> {
            recorder* rec = nullptr;
            lweh::intrusive_signal<dummy_event>* sig = nullptr;
            lweh::intrusive_node<dummy_event>* to_attach = nullptr;
            void on_event(const dummy_event&) override {
                rec->record('X');
                sig->attach(*to_attach);
            }
        };
        attacher_node x;
        x.rec = &r;
        x.to_attach = &new_node;

        lweh::intrusive_signal<dummy_event> s;
        x.sig = &s;
        s.attach(x);

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 1);
        LWEH_EXPECT_EQ(r.trace[0], 'X'); // new_node must NOT fire this pass

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 3);
        LWEH_EXPECT_EQ(r.trace[1], 'N'); // fires first next pass (head-inserted)
        LWEH_EXPECT_EQ(r.trace[2], 'X');
    }

    // 6. Documented known caveat, pinned as expected behavior (not a
    // regression if this exact trace holds; IS a regression if it silently
    // changes): self-detach followed by detaching one's own frozen
    // not-yet-visited successor, in that order in one callback, spuriously
    // refires the successor once this same pass despite its detach() call
    // legitimately returning true (detail::intrusive_core's doc comment;
    // confirmed by adversarial stress-testing before this was written).
    // Reversing the order (detach the other node first, then self-detach)
    // avoids it -- not exercised here since this test's whole point is
    // pinning down the documented caveat itself.
    {
        recorder r;
        basic_node successor;
        successor.tag = 'Y';
        successor.rec = &r;

        struct self_then_other_node : lweh::intrusive_node<dummy_event> {
            recorder* rec = nullptr;
            lweh::intrusive_signal<dummy_event>* sig = nullptr;
            lweh::intrusive_node<dummy_event>* successor_node = nullptr;
            bool detach_result = false;
            void on_event(const dummy_event&) override {
                rec->record('X');
                sig->detach(*this);                            // self-detach first
                detach_result = sig->detach(*successor_node);  // then own frozen successor
            }
        };
        self_then_other_node x;
        x.rec = &r;
        x.successor_node = &successor;

        lweh::intrusive_signal<dummy_event> s;
        x.sig = &s;
        s.attach(successor); // attached first -> becomes x's successor once x attaches
        s.attach(x);          // attached second -> head_; list is x -> successor

        s.publish(dummy_event{1});
        LWEH_EXPECT(x.detach_result);        // detach(successor) legitimately reports success...
        LWEH_EXPECT_EQ(r.count, 2);          // ...yet successor still fires once more (documented caveat)
        LWEH_EXPECT_EQ(r.trace[0], 'X');
        LWEH_EXPECT_EQ(r.trace[1], 'Y');

        s.publish(dummy_event{1}); // list is fully correct starting next pass: nobody left
        LWEH_EXPECT_EQ(r.count, 2);
    }

    // 7. Detach the head node directly (outside dispatch): the new head must
    // become the old head's immediate successor, and the remaining nodes
    // must still dispatch correctly afterward. Every existing head-removal
    // case above is entangled with self-detach-during-dispatch reentrancy
    // (tests 3 and 6); this isolates the plain unlink() head_==&node branch
    // on its own, called from an ordinary (non-reentrant) context.
    {
        recorder r;
        basic_node a;
        a.tag = 'A';
        a.rec = &r;
        basic_node b;
        b.tag = 'B';
        b.rec = &r;
        basic_node c;
        c.tag = 'C';
        c.rec = &r;

        lweh::intrusive_signal<dummy_event> s;
        s.attach(a); // list becomes, head to tail: A
        s.attach(b); // B -> A
        s.attach(c); // C -> B -> A (c is head)

        LWEH_EXPECT(s.detach(c)); // detach the head directly, outside dispatch

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 2);
        LWEH_EXPECT_EQ(r.trace[0], 'B'); // new head fires first
        LWEH_EXPECT_EQ(r.trace[1], 'A');
        LWEH_EXPECT_EQ(c.hits, 0); // detached before publish; must not fire
    }

    // 8. Detach a genuine middle node directly (outside dispatch) from a
    // 3-node list -- neither head nor tail. No existing test exercises this:
    // test 2's non-head detach is the tail of a 2-node list, and test 4's
    // during-dispatch detach targets the tail of a 3-node list. This pins
    // down unlink()'s list-walk branch relinking around an interior node.
    {
        recorder r;
        basic_node a;
        a.tag = 'A';
        a.rec = &r;
        basic_node b;
        b.tag = 'B';
        b.rec = &r;
        basic_node c;
        c.tag = 'C';
        c.rec = &r;

        lweh::intrusive_signal<dummy_event> s;
        s.attach(a); // A
        s.attach(b); // B -> A
        s.attach(c); // C -> B -> A (b is the middle node)

        LWEH_EXPECT(s.detach(b)); // detach the middle node directly

        s.publish(dummy_event{1});
        LWEH_EXPECT_EQ(r.count, 2);
        LWEH_EXPECT_EQ(r.trace[0], 'C'); // head unaffected
        LWEH_EXPECT_EQ(r.trace[1], 'A'); // c now points directly to a
        LWEH_EXPECT_EQ(b.hits, 0); // detached; must not fire
    }

    return lweh_test::finish();
}
