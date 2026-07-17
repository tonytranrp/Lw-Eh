#pragma once
#ifndef LWEH_DELEGATE_HPP_INCLUDED
#define LWEH_DELEGATE_HPP_INCLUDED

namespace lweh {

// Non-owning, fixed 2-pointer callback type (research.md Part A §A1):
// a void* context plus a templated static stub generated at the bind call
// site (Ryazanov "impossibly fast" delegate pattern). No std::function, no
// heap, no vtable, no throw path.
//
// TODO(Phase 1): bind<Fn>() for free functions, bind<MemFn>(T*) for member
// functions, operator()(Args...) to invoke.
template <typename Sig>
class delegate;

template <typename Ret, typename... Args>
class delegate<Ret(Args...)> {
public:
    // TODO(Phase 1): implement.
};

} // namespace lweh

#endif // LWEH_DELEGATE_HPP_INCLUDED
