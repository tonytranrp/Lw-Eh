#pragma once
#ifndef LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED
#define LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED

namespace lweh::detail {

// Shared non-template dispatch engine, as originally scaffolded in Phase 0
// (research.md Part A §A4; ARCHITECTURE.md's original directory tree listed
// signal.hpp as "a thin shim over detail::signal_core"). That plan was
// deliberately amended, not followed through on: signal<Event, MaxListeners>
// (see signal.hpp's own class-level comment) stores its array directly and
// does NOT use this type, because event-erasing the dispatch loop would need
// an extra per-event adapter-stub layer on top of delegate's own stub, for a
// size win research.md §A4 itself says only pays off "past small single
// digits" of event types -- which this codebase doesn't have yet (§A10:
// measure before optimizing, not before). This stub is kept, empty and
// unimplemented ON PURPOSE, as a placeholder for that future revisit -- not
// outstanding Phase-1 work-in-progress. See ARCHITECTURE.md's Phase-0
// amendment section for the full reasoning behind why it was shelved.
//
// If ever revisited: bounds-checked attach/detach over a type-erased slot
// array pointer + count (will need <cstddef> for std::size_t), and a
// bounded-loop dispatch(...) that invokes each slot.
class signal_core {
    // Deliberately empty -- see the comment above. Nothing in the library
    // currently uses this type.
};

} // namespace lweh::detail

#endif // LWEH_DETAIL_SIGNAL_CORE_HPP_INCLUDED
