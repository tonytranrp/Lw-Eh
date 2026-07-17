#pragma once
#ifndef LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED
#define LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED

namespace lweh::detail {

// Shared non-template engine backing intrusive_signal<Event>: an intrusive
// singly-linked list where the listener object itself is the node, so the
// signal's own footprint is a single head pointer regardless of listener
// count (research.md §A7 alternate storage policy — O(1) insert, O(n)
// unlink, zero allocation).
//
// TODO(Phase 1): implement over a type-erased node-with-next-pointer + a
// virtual invoke hook (see intrusive_signal.hpp).
class intrusive_core {
public:
    // TODO(Phase 1): implement.
};

} // namespace lweh::detail

#endif // LWEH_DETAIL_INTRUSIVE_CORE_HPP_INCLUDED
