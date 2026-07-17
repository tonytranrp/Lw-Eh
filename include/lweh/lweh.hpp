#pragma once
#ifndef LWEH_LWEH_HPP_INCLUDED
#define LWEH_LWEH_HPP_INCLUDED

// Umbrella convenience header — pulls in the full public API. Prefer
// including only what you use directly in size-critical translation units
// (research.md §A5); this header exists for convenience during development
// and for consumers who don't care about the difference.

#include "config.hpp"
#include "delegate.hpp"
#include "detail/signal_core.hpp"
#include "signal.hpp"
#include "detail/intrusive_core.hpp"
#include "intrusive_signal.hpp"

#endif // LWEH_LWEH_HPP_INCLUDED
