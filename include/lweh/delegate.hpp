#pragma once
#ifndef LWEH_DELEGATE_HPP_INCLUDED
#define LWEH_DELEGATE_HPP_INCLUDED

#include <type_traits>

namespace lweh {

// Non-owning, fixed 2-pointer callback (research.md Part A §A1): a void*
// object pointer plus a function pointer to a compiler-generated stub
// trampoline, selected via a C++17 `auto` non-type template parameter. The
// bound function/member-function pointer is never stored or reinterpret_cast
// — it only ever exists as a compile-time template argument baked into which
// stub instantiation gets its address taken (Ryazanov "impossibly fast"
// delegate pattern; deliberately not Clugston's variant, which relies on
// unspecified pointer-to-member-function representation).
//
// Default-constructs into an unbound state (stub_ == nullptr) so it can live
// in a plain array as signal<Event,N>'s slot storage — unlike a strict
// function_ref, which has no empty state. Calling an unbound delegate is
// undefined behavior, exactly like calling through a null function pointer;
// check operator bool() first if that's possible at a given call site.
template <typename Sig>
class delegate;

template <typename Ret, typename... Args>
class delegate<Ret(Args...)> {
    using stub_t = Ret (*)(void*, Args...);

    void* obj_ = nullptr;
    stub_t stub_ = nullptr;

    template <auto Fn>
    static Ret free_function_stub(void*, Args... args) {
        static_assert(!std::is_member_pointer_v<decltype(Fn)>,
                      "bind<Fn>() is for free functions only; use bind<MemFn>(instance) for member functions.");
        // Direct call syntax deliberately, not std::invoke: std::invoke would
        // treat a mistakenly-passed member-function pointer as an "unbound
        // member" call consuming Args...[0] as the implicit object instead of
        // failing to compile — a silent-wrong-behavior footgun this design
        // avoids on purpose (Lw-Eh has no "unbound member" binding mode).
        return Fn(args...);
    }

    template <auto MemFn, typename T>
    static Ret member_function_stub(void* p, Args... args) {
        // T is deduced from the caller's instance pointer, not forced to
        // MemFn's declaring class, so binding a method inherited from a base
        // of T works correctly with no special-casing.
        return (static_cast<T*>(p)->*MemFn)(args...);
    }

public:
    delegate() = default;

    // Bind a free function: delegate<Ret(Args...)>::bind<&some_function>().
    template <auto Fn>
    void bind() {
        obj_ = nullptr;
        stub_ = &free_function_stub<Fn>;
    }

    // Bind a member function on a specific instance:
    // delegate<Ret(Args...)>::bind<&Class::method>(&instance).
    template <auto MemFn, typename T>
    void bind(T* instance) {
        obj_ = instance;
        stub_ = &member_function_stub<MemFn, T>;
    }

    Ret operator()(Args... args) const {
        return stub_(obj_, args...);
    }

    explicit operator bool() const {
        return stub_ != nullptr;
    }

    // Equality is "bound to the same target": same instance pointer (or both
    // null, for free functions) and the same stub. Used by signal<Event,N>
    // to find a specific listener to detach (Research/ARCHITECTURE.md:
    // detach<Fn>()/detach<MemFn>(T*) mirror bind's own syntax, matched by
    // reconstructed value rather than a returned handle).
    //
    // Narrow known caveat, not a correctness bug in normal use: under
    // aggressive identical-code-folding (GCC's -fipa-icf, on by default at
    // -Os/-O2, or an explicit linker --icf=all), two DIFFERENT member
    // functions that happen to compile to byte-identical code and are bound
    // to the SAME instance could fold to the same stub_ address and compare
    // equal here even though they're conceptually different bindings. This
    // only matters if delegate equality is later used for something where
    // that distinction is safety-relevant (it isn't, for detach-by-value).
    friend bool operator==(const delegate& lhs, const delegate& rhs) {
        return lhs.obj_ == rhs.obj_ && lhs.stub_ == rhs.stub_;
    }
    friend bool operator!=(const delegate& lhs, const delegate& rhs) {
        return !(lhs == rhs);
    }
};

// Permanent, zero-cost regression guard: if a future change accidentally
// grows delegate past 2 pointers, every build fails here immediately rather
// than the regression being caught later by a size-audit diff.
static_assert(sizeof(delegate<void()>) == sizeof(void*) * 2,
              "lweh::delegate must stay exactly 2 pointers (research.md Part A §A1).");

} // namespace lweh

#endif // LWEH_DELEGATE_HPP_INCLUDED
