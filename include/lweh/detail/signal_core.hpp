#pragma once
#ifndef LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED
#define LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED

namespace lweh::detail {

// Shared non-template dispatch engine (research.md Part A §A4 — the single
// highest-leverage size decision in the whole design). Compiled once;
// signal<Event, N> is a thin template shim that forwards into this with the
// right cast, instead of every event type instantiating its own full
// attach/detach/dispatch logic.
//
// TODO(Phase 1): bounds-checked attach/detach over a type-erased slot array
// pointer + count (will need <cstddef> for std::size_t once implemented),
// and a bounded-loop dispatch(...) that invokes each slot.
class signal_core {
public:
    // TODO(Phase 1): implement.
};

} // namespace lweh::detail

#endif // LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED
