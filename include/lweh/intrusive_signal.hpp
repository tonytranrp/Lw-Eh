#pragma once
#ifndef LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED
#define LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED

#include "detail/intrusive_core.hpp"

namespace lweh {

template <typename Event>
class intrusive_signal;

// intrusive_node<Event>: mixin base a listener inherits from and overrides
// on_event(). Deliberately a separate type from signal's array-backed
// listeners rather than a storage-policy template parameter on signal<> —
// the two storage strategies have genuinely different call-site contracts
// (array-backed requires nothing from the listener; this requires
// inheriting a base), so folding both into one template would add an axis
// of generality most users never touch (Research/ARCHITECTURE.md).
//
// No auto-detach on destruction, and no double-attach guard: a node
// destroyed while still linked, or attached to a second signal while
// already linked to one, is caller error (a dangling/malformed list) —
// this is a deliberate tradeoff matching Boost.Intrusive's default
// (non-auto_unlink) hooks and the Linux kernel's plain list_head, not
// something engineered around here (research.md §A7's own stated
// tradeoff: "listener lifetime becomes the caller's responsibility").
//
// See detail::intrusive_core for the full dispatch-safety contract this
// type inherits, including one narrow, documented, self-healing reentrancy
// caveat found by adversarial stress-testing before this was written.
template <typename Event>
class intrusive_node : private detail::intrusive_link {
    void invoke_erased(const void* event) override {
        on_event(*static_cast<const Event*>(event));
    }

public:
    virtual void on_event(const Event& event) = 0;

protected:
    intrusive_node() = default;
    // Protected AND non-virtual, matching detail::intrusive_link's own
    // destructor (detail/intrusive_core.hpp) one layer down. The access
    // level alone is what makes this safe without vtable-destructor
    // overhead: `delete` through an `intrusive_node<Event>*` base pointer
    // from outside this class hierarchy is a COMPILE ERROR (protected
    // members aren't callable from outside), not a documented convention
    // callers have to trust themselves to follow -- confirmed empirically
    // (firing 32): attempting exactly that fails with "calling a protected
    // destructor of class ...". Only the caller's own most-derived listener
    // type can ever be deleted, which is always safe without a virtual
    // destructor since destruction then starts from the correct, most-
    // derived type by construction, not by convention.
    ~intrusive_node() = default;

private:
    friend class intrusive_signal<Event>;
};

// intrusive_signal<Event>: thin template shim over detail::intrusive_core.
// Unlike signal<Event,N>, this genuinely shares one non-template compiled
// engine across every Event type (research.md §A4) — virtual dispatch
// erases Event "for free" via the vtable, without needing signal<>'s
// deferred delegate-adapter-stub approach.
template <typename Event>
class intrusive_signal : private detail::intrusive_core {
public:
    intrusive_signal() = default;

    void attach(intrusive_node<Event>& node) {
        link(node);
    }

    // Returns false (no-op) if node isn't currently attached to this signal.
    bool detach(intrusive_node<Event>& node) {
        return unlink(node);
    }

    // Invoke every currently-attached listener with e, in LIFO order (most
    // recently attached fires first) — a consequence of O(1) head-insertion
    // attach, and a real behavioral difference from signal<Event,N>'s FIFO
    // (attach-order) dispatch. See detail::intrusive_core's class-level
    // comment for the full dispatch-safety contract, including reentrant
    // attach/detach.
    void publish(const Event& e) {
        dispatch(&e);
    }
};

} // namespace lweh

#endif // LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED
