#pragma once
#ifndef LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED
#define LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED

namespace lweh::detail {

// Non-template node base every intrusive-list listener inherits from (via
// intrusive_node<Event>, one layer up in intrusive_signal.hpp). The single
// virtual invoke_erased(const void*) is what makes intrusive_core below
// genuinely Event-erased: virtual dispatch provides the type erasure "for
// free" via the vtable, unlike signal<Event,N>'s array-based delegate
// storage, which would need an extra adapter-stub layer to erase Event the
// same way (research.md §A4; Research/ARCHITECTURE.md's deferred-
// core-extraction note explains why that path wasn't taken for signal<>).
struct intrusive_link {
    intrusive_link* next = nullptr;
    virtual void invoke_erased(const void* event) = 0;

protected:
    // Non-virtual on purpose: nothing ever deletes through an
    // intrusive_link*, only through the most-derived listener type the
    // caller owns.
    ~intrusive_link() = default;
};

// Shared non-template engine (research.md §A4) — exactly one compiled copy
// regardless of how many distinct Event types use intrusive_signal<Event>.
// Singly-linked, head-insertion, O(1) attach / O(n) detach — deliberately
// not doubly-linked, to keep the per-listener cost to exactly one pointer
// (research.md §A7's stated tradeoff for this storage policy).
//
// Dispatch-safety contract — verified by adversarial stress-testing
// (compiled, ASan/UBSan-checked) before this was written:
//   - A listener may detach itself, or any other already-linked listener
//     (already-fired or not-yet-fired this pass, adjacent or not), from
//     within its own callback. This is always safe on its own: no crash,
//     no double-invoke, no incorrectly-skipped listener.
//   - A listener attached during an in-progress dispatch() is *never*
//     invoked during that same pass — unconditionally, regardless of when
//     in the pass the attach happens (unlike signal<Event,N>'s array-based
//     contract, which is slot-index/cursor-dependent, this one is simple
//     and timing-independent: head-insertion only ever affects nodes
//     reachable from head_, and the traversal's cursor never re-reads
//     head_ after starting).
//   - KNOWN CAVEAT, narrow and self-healing, not a memory-safety issue: if
//     a listener both self-detaches AND detaches a different listener that
//     was its own not-yet-visited immediate successor at that moment, IN
//     THAT ORDER within one callback, the second listener may spuriously
//     fire once more this same pass despite its detach() call having
//     already returned true. This happens because the self-detaching
//     node's own `next` field is deliberately left untouched by unlink()
//     (that's what makes plain self-detach safe per the first bullet
//     above), so it becomes a stale snapshot if the list changes again
//     independently before the dispatch loop reads it. The list itself is
//     fully correct starting the very next dispatch() — this is a
//     same-pass-only quirk. Avoid it by detaching the OTHER listener
//     BEFORE self-detaching, not after, when both happen in one callback.
//   - None of the above is safe against concurrent/ISR reentrancy — same
//     caveat as signal<Event,N> (research.md §A9).
class intrusive_core {
protected:
    intrusive_link* head_ = nullptr;

    void link(intrusive_link& node) {
        node.next = head_;
        head_ = &node;
    }

    // Returns false (no-op) if node isn't currently linked in this list.
    bool unlink(intrusive_link& node) {
        if (head_ == &node) {
            head_ = node.next;
            return true;
        }
        for (intrusive_link* n = head_; n; n = n->next) {
            if (n->next == &node) {
                n->next = node.next;
                return true;
            }
        }
        return false;
    }

    void dispatch(const void* event) const {
        for (intrusive_link* n = head_; n; n = n->next) {
            n->invoke_erased(event);
        }
    }
};

} // namespace lweh::detail

#endif // LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED
