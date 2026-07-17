#pragma once
#ifndef LWEH_SIGNAL_HPP_INCLUDED
#define LWEH_SIGNAL_HPP_INCLUDED

#include <cstddef>

#include "config.hpp"
#include "delegate.hpp"

namespace lweh {

// signal<Event, MaxListeners>: fixed-capacity, zero-allocation multi-listener
// event dispatcher for one event type. Storage is a plain array of
// delegate<void(const Event&)> slots; occupancy is tracked per-slot via the
// delegate's own operator bool() (mark-and-skip) rather than a separate live
// count. Each event type is its own compile-time-bound signal — no runtime
// type id, no lookup, no registry (research.md §A2/§A10;
// Research/ARCHITECTURE.md Reconciliation 3).
//
// Dispatch-safety contract — verified empirically (compiled, sanitizer-
// checked stress tests) before this was written, not assumed:
//   - A listener may detach itself, or any other listener (already-fired or
//     not-yet-fired this pass), from within its own publish() callback. This
//     is always safe: no crash, no double-invoke, no incorrectly-skipped
//     listener. A not-yet-fired listener that gets detached simply does not
//     fire in that same pass.
//   - A listener attached during an in-progress publish() may or may not be
//     invoked during that same call: it fires in the current pass iff the
//     free slot it lands in has a higher index than the slot currently being
//     dispatched (which depends on prior attach/detach history, not
//     registration order) — otherwise it's deferred to the next publish().
//   - None of the above is safe against concurrent/ISR reentrancy (calling
//     attach/detach/publish from an interrupt while another is in progress
//     on the main thread) — that needs the caller's own synchronization,
//     deliberately not baked in here (research.md §A9; the Boost.Signals2
//     lesson in §A6: never make thread-safety an unconditional default cost).
//
// detail::signal_core's dispatch-sharing role from research.md §A4 is
// deliberately deferred, not abandoned: making the dispatch loop genuinely
// event-erased would need an extra per-event adapter-stub layer on top of
// delegate's own stub, for a size win that §A4 itself says only pays off
// "past small single digits" of event types — which this codebase doesn't
// have yet. Revisit once there's real multi-event-type size data to justify
// it (§A10: measure before optimizing), not before.
template <typename Event, std::size_t MaxListeners = default_max_listeners>
class signal {
    using slot_t = delegate<void(const Event&)>;
    slot_t slots_[MaxListeners]{};

    bool detach_matching(const slot_t& target) {
        for (std::size_t i = 0; i < MaxListeners; ++i) {
            if (slots_[i] == target) {
                slots_[i] = slot_t{};
                return true;
            }
        }
        return false;
    }

public:
    signal() = default;

    // Attach a free function. Returns false (no exceptions, per research.md
    // §A3) if all MaxListeners slots are already occupied.
    template <auto Fn>
    bool attach() {
        for (std::size_t i = 0; i < MaxListeners; ++i) {
            if (!slots_[i]) {
                slots_[i].template bind<Fn>();
                return true;
            }
        }
        return false;
    }

    // Attach a member function bound to instance. Same capacity contract as
    // the free-function overload above.
    template <auto MemFn, typename T>
    bool attach(T* instance) {
        for (std::size_t i = 0; i < MaxListeners; ++i) {
            if (!slots_[i]) {
                slots_[i].template bind<MemFn>(instance);
                return true;
            }
        }
        return false;
    }

    // Detach a free function previously attached with the same Fn. Returns
    // false (no-op) if it isn't currently attached.
    template <auto Fn>
    bool detach() {
        slot_t target;
        target.template bind<Fn>();
        return detach_matching(target);
    }

    // Detach a member function previously attached with the same MemFn and
    // instance. Returns false (no-op) if it isn't currently attached.
    template <auto MemFn, typename T>
    bool detach(T* instance) {
        slot_t target;
        target.template bind<MemFn>(instance);
        return detach_matching(target);
    }

    // Invoke every currently-attached listener with e, in slot order. Safe
    // to attach/detach (including self-detach) from within a listener called
    // during this pass — see the class-level dispatch-safety contract above.
    void publish(const Event& e) {
        for (std::size_t i = 0; i < MaxListeners; ++i) {
            if (slots_[i]) {
                slots_[i](e);
            }
        }
    }
};

} // namespace lweh

#endif // LWEH_SIGNAL_HPP_INCLUDED
