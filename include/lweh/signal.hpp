#pragma once
#ifndef LWEH_SIGNAL_HPP_INCLUDED
#define LWEH_SIGNAL_HPP_INCLUDED

#include <cstddef>

#include "config.hpp"
#include "detail/signal_core.hpp"

namespace lweh {

// signal<Event, MaxListeners>: thin template shim over detail::signal_core,
// owning a fixed-capacity static array of delegate<void(const Event&)> slots
// — the default storage policy (research.md §A7). Zero allocation, known
// sizeof at compile time, lives in .bss/on the stack.
//
// Each event type is its own compile-time-bound signal — no runtime type id,
// no lookup, no registry (research.md §A2: deliberately not EnTT's
// dispatcher; see Research/ARCHITECTURE.md Reconciliation 3).
//
// TODO(Phase 1): #include "delegate.hpp" and add a
// delegate<void(const Event&)> slots_[MaxListeners] member; attach<Fn>() /
// attach<MemFn>(T*) -> bool (false if at capacity, no exceptions per §A3),
// detach<...>(), publish(const Event&).
template <typename Event, std::size_t MaxListeners = default_max_listeners>
class signal : private detail::signal_core {
public:
    // TODO(Phase 1): implement.
};

} // namespace lweh

#endif // LWEH_SIGNAL_HPP_INCLUDED
