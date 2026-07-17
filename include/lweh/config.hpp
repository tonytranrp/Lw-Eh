#pragma once
#ifndef LWEH_CONFIG_HPP_INCLUDED
#define LWEH_CONFIG_HPP_INCLUDED

#include <cstddef>

#define LWEH_VERSION_MAJOR 0
#define LWEH_VERSION_MINOR 1
#define LWEH_VERSION_PATCH 0

namespace lweh {

// Default listener capacity for signal<Event, MaxListeners> when the caller
// doesn't pick one explicitly. A template default, not a #define — the
// capacity is part of the type (research.md Part A §A5 ODR-safety rule).
inline constexpr std::size_t default_max_listeners = 4;

} // namespace lweh

#endif // LWEH_CONFIG_HPP_INCLUDED
