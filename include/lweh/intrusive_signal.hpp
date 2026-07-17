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
// One specific consequence of the above is severe enough to call out on
// its own, empirically confirmed (Research/PROGRESS.md), not just implied
// by "malformed list": attaching the SAME node to the SAME signal twice
// makes that node's own next pointer reference itself, and publish()
// afterward loops FOREVER — a genuine hang, not merely corrupted or lost
// data the way the cross-signal case is. This isn't a gap this library
// engineers around either, for the same reason the cases above aren't:
// detecting it cheaply would need real per-node state (an "already
// linked" flag or sentinel) this design deliberately doesn't carry, to
// keep the per-listener cost to exactly one pointer (detail::intrusive_
// core's own stated goal) — Boost.Intrusive's own safe_link/auto_unlink
// modes, which DO detect this, pay for exactly that extra state, and
// Lw-Eh's default (matching Boost's own default, non-safe) mode doesn't
// carry it either. Attach each node to at most one signal at a time.
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
//
// Precision note (firing 34, real disassembly on 2 real Event-type
// instantiations, not assumed): "shares one compiled engine" is fully true
// for dispatch()/publish() -- detail::intrusive_core::dispatch() confirmed
// as exactly one shared symbol, the intrusive-storage analog of firing 18's
// finding that signal<>::publish() gets folded across event types by IPA-ICF.
// It is NOT true the same way for attach()/detach(): link()/unlink() are
// small enough that GCC inlines them fully at each call site rather than
// emitting (and then folding) a standalone call, the same "always inlined,
// never shared" behavior firing 18 found for signal<>::attach(). The net
// marginal cost per additional intrusive_signal<>-backed Event type is still
// meaningfully smaller than signal<>'s (measured: 96 bytes vs. the
// established ~143-146 bytes/type, Research/PROGRESS.md) because dispatch()
// is genuinely free and link()/unlink()'s inlined bodies are tiny (a few
// instructions) compared to signal<>'s scan-for-empty-slot loop -- but it is
// not literally zero marginal cost the way the sentence above could be
// read to imply.
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
