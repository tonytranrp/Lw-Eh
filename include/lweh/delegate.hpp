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
    //
    // The is_invocable_r_v check below and the is_member_pointer_v check
    // inside free_function_stub (above) are deliberately BOTH kept, not one
    // in place of the other -- they catch different, complementary mistakes.
    // is_invocable_r_v rejects wrong arity/parameter types early, right here
    // at the call site, before free_function_stub<Fn> is even instantiated.
    // But it uses the same INVOKE semantics std::invoke does, so it has the
    // identical loophole this file's own std::invoke-avoidance comment
    // (see free_function_stub) warns about: a member-function pointer whose
    // shape happens to match "call this with Args...[0] as the implicit
    // object" (e.g. binding a zero-arg method of Event itself to a
    // delegate<void(const Event&)>) satisfies is_invocable_r_v and slips
    // past this check undetected. free_function_stub's unconditional
    // is_member_pointer_v assert is the only complete backstop for that
    // case -- it's a pure type check, immune to the invocability loophole --
    // which is exactly why it stays, unconditionally, even with this added.
    // (Investigated via 2 parallel subagents before implementing: C++20
    // `requires` was considered and rejected -- confirmed to actually make
    // the common-mistake diagnostic WORSE, not better, since a failed
    // requires-clause's generic "constraints not satisfied" message
    // pre-empts the friendlier static_assert text below via SFINAE, and it
    // would have added permanent C++17/C++20 dual-path complexity for a
    // benefit a plain static_assert already gets on the guaranteed C++17
    // floor. Research/PROGRESS.md has the full writeup.)
    template <auto Fn>
    void bind() {
        static_assert(std::is_invocable_r_v<Ret, decltype(Fn), Args...>,
                      "bind<Fn>(): Fn is not callable as Ret(Args...).");
        obj_ = nullptr;
        stub_ = &free_function_stub<Fn>;
    }

    // Bind a member function on a specific instance:
    // delegate<Ret(Args...)>::bind<&Class::method>(&instance).
    //
    // Unlike the free-function overload above, MemFn had NO compile-time
    // signature check at all before this: a wrong arity/type failed deep
    // inside member_function_stub's body with a raw template-instantiation
    // error. This closes that gap for the common case. Known, narrow,
    // honestly-documented limitation (not engineered around, matching how
    // delegate's own equality operator documents its ICF-folding caveat
    // rather than pretending it doesn't exist): a plain FREE function
    // shaped exactly like Ret(T*, Args...) -- taking T* as its first
    // parameter -- satisfies is_invocable_r_v here too, via the same
    // INVOKE-based loophole described above, even though it isn't actually
    // a pointer-to-member and member_function_stub's `(p->*MemFn)(args...)`
    // would reject it outright once instantiated. Considered too narrow and
    // deliberate a mistake (binding a free function through the member-
    // function overload, with a first parameter that happens to match the
    // instance type exactly) to be worth a second guard for.
    template <auto MemFn, typename T>
    void bind(T* instance) {
        static_assert(std::is_invocable_r_v<Ret, decltype(MemFn), T*, Args...>,
                      "bind<MemFn>(instance): MemFn is not callable as Ret(Args...) on *instance.");
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
