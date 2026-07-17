#pragma once
#ifndef LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED
#define LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED

#include "detail/intrusive_core.hpp"

namespace lweh {

// intrusive_node<Event>: mixin base a listener inherits from and overrides
// on_event(). Deliberately a separate type from signal's array-backed
// listeners rather than a storage-policy template parameter on signal<> —
// the two storage strategies have genuinely different call-site contracts
// (array-backed requires nothing from the listener; this requires
// inheriting a base), so folding both into one template would add an axis
// of generality most users never touch (Research/ARCHITECTURE.md).
//
// TODO(Phase 1): next-pointer + list linkage, managed by intrusive_core.
template <typename Event>
class intrusive_node {
public:
    virtual void on_event(const Event& event) = 0;

protected:
    ~intrusive_node() = default;
    // TODO(Phase 1): next-pointer.
};

// intrusive_signal<Event>: thin template shim over detail::intrusive_core.
// TODO(Phase 1): attach(intrusive_node<Event>&), detach(...), publish(const Event&).
template <typename Event>
class intrusive_signal : private detail::intrusive_core {
public:
    // TODO(Phase 1): implement.
};

} // namespace lweh

#endif // LWEH_INTRUSIVE_SIGNAL_HPP_INCLUDED
