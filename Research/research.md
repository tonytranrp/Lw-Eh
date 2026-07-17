# Lw-Eh Research: Building a 1-2KB, Zero-Dependency C++ Event Handler for Embedded Targets

**Status:** Pre-implementation research. No code has been written yet — this document exists to be reviewed before architecture/implementation decisions are locked in.

## Project goal & hard constraints (recap)

Lw-Eh ("Light-Weight Event Handler") is a from-scratch C++ event/signal-dispatch library — conceptually in the spirit of EnTT's `entt::sigh`/`entt::dispatcher` (type-safe observer/pub-sub), but EnTT is considered too large and too general for this project's goals. Lw-Eh is a radically stripped-down alternative, built under these non-negotiable constraints:

- **Language:** C++, built with **CMake**.
- **Binary footprint target: 1-2 KB compiled.** Every design and build decision is judged against this.
- **Zero external dependencies.** Nothing fetched — no `FetchContent`, no vcpkg/Conan, no Boost, no other event library. Everything handwritten.
- **No dynamic linking / minimal link overhead.** Target deployment is embedded microcontrollers (ESP32 and similar "circuit devices"). The library must behave as something that compiles straight into firmware, not a shared-object dependency.
- **Compile time is irrelevant.** Only the size/quality/performance of the final compiled output matters — optimize purely for output, never for build speed.

This research was produced by two independent research agents — one focused on **C++ language/architecture techniques** (Part A), one focused on **CMake/compiler/linker toolchain setup** (Part B) — run without access to the deep-research skill or the ability to spawn further subagents, per the project owner's request. Findings are merged below with a synthesis up front.

---

## Key cross-cutting takeaways

These are the points where both research tracks independently converged, or where one track's finding materially changes how to read the other's. Read this section first; Parts A and B below are the full reference detail.

1. **The single biggest threat to the 1-2KB budget isn't the dispatcher logic — it's accidentally pulling in "normal" STL.** `<iostream>` alone costs ~200KB on ESP-IDF. Leaving RTTI enabled costs "tens of KB." At this budget, an incidental `#include <functional>` or one polymorphic class that slips through is an *existential* risk, not a rounding error. ESP-IDF already disables exceptions and RTTI by default — the job is to not turn them back on and to avoid `std::function`/`std::vector`/`std::string`/`std::map`/`<iostream>`/`<sstream>`/`<regex>`/`<thread>` entirely in the library's own headers.

2. **The highest-leverage architecture decision, named independently by both research tracks: don't let templates duplicate the whole engine per event type.** A naive design where every `signal<EventA>`, `signal<EventB>`, `signal<EventC>` independently instantiates its *entire* attach/detach/dispatch logic grows size roughly linearly with event-type count (~150-400 bytes each). The fix is a **shared non-template core** (compiled once) with a **thin template shim** per event type that just forwards with the right cast (~20-80 bytes each, often inlines away almost completely). Past a handful of event types, this is close to mandatory for staying in budget — see Part A §4 and §10 for the full breakdown.

3. **Delegate/listener storage should be a non-owning, fixed 2-pointer struct — not `std::function`.** The recommended shape is the "impossibly fast" pattern (function-pointer + `void*` context, with a templated static stub generated at the bind call site): 2 pointers, no heap, no vtable, no throw path, and small enough to pass in registers on Xtensa/RISC-V calling conventions. See Part A §1.

4. **Storage model default: fixed-capacity static array sized by a compile-time template parameter, not `std::vector`.** An intrusive linked list is a legitimate alternate policy (shifts cost onto the listener, useful when the dispatcher itself must be cheaply embeddable in many places), but for code-size-first goals the static array's simpler, tighter dispatch loop wins by default. See Part A §7.

5. **Realistic size framing: 1-2KB is a budget for "the shared mechanism plus a handful of event types," not an absolute ceiling regardless of how many event types get declared.** A single trivial event type realistically costs 200-500 bytes with room to spare. A general multi-event-type dispatcher can plausibly cover 5-15 event types within 1-2KB **if** the shared-core pattern (#2 above) is adopted from the start; under a naive fully-templated design, the same budget covers only 2-5 event types before it's exhausted. Treat the number as "budget for the mechanism, small marginal cost per additional type," and validate with real measurements early rather than trusting any estimate (including this one) — see Part A §10 and Part B §9 for the measurement workflow.

6. **On the build side, "no dependencies fetched" is achieved by omission, not a flag.** There's no CMake "offline mode" — the guarantee comes from the fact that `FetchContent`, `ExternalProject_Add`, `find_package`, and any vcpkg/Conan integration simply never appear anywhere in Lw-Eh's CMake files. A CI job with no network access, plus a grep-based lint gate, is how you prove it rather than just assert it. See Part B §1.

7. **"No linking overhead" has two genuinely different halves.** (A) Lw-Eh itself, as a CMake `INTERFACE` library with zero compiled sources, adds *zero* entries to the consumer's link command — fully achievable and easy to verify (`INTERFACE_LINK_LIBRARIES` should be empty). (B) The platform toolchain will *always* link a minimal startup/`libgcc`/libc runtime regardless of whether Lw-Eh is used at all — that floor is not attributable to Lw-Eh and shouldn't be measured against its budget. The correct way to measure Lw-Eh's true footprint is a **diff**: build firmware with and without Lw-Eh and compare (Bloaty McBloatface's diff mode is built for exactly this). See Part B §6 and §9.

8. **GCC is the safe default for Xtensa (ESP32) today; Clang/LLVM is real but not yet production-proven there.** Espressif is actively investing in LLVM for Xtensa, but as of this research the assembler/linker side still leans on binutils rather than a clean LLVM pipeline, and ESP-IDF's Clang support (`IDF_TOOLCHAIN=clang`) is explicitly labeled experimental. By contrast, **RISC-V ESP32 variants (C3/C6) are natively supported in mainline LLVM**, so Clang is a materially safer bet there than on classic Xtensa chips. On Cortex-M generally, GCC and Clang are roughly comparable (within a few percent) in modern (2020s) toolchain versions. See Part B §7.

9. **The C library picture for ESP32 recently changed.** The "classic" advice is newlib + `--specs=nano.specs` (newlib-nano). But ESP-IDF has moved past that: **as of ESP-IDF v6.0, the project-wide default libc is PicolibC**, reported smaller than even newlib-with-nano-formatting. This supersedes older newlib-nano-only advice for ESP32 specifically (AVR remains on its own separate avr-libc lineage, unaffected). See Part B §8.

10. **C++ standard: target C++17 as the guaranteed-portable floor**, with C++20 usable opportunistically behind feature-test macros. Arduino-ESP32 currently sits around C++20 (`gnu++2a`), raw ESP-IDF defaults much newer (`gnu++26` as of current stable docs) — but that reflects "whatever GCC's newest GNU dialect happens to be," not a stable feature floor to depend on. C++17 clears every relevant toolchain in scope (ESP-IDF, Arduino-ESP32, ARM-GCC, AVR-GCC) and already buys `if constexpr`, `inline` variables (needed for the header-only promise), and relaxed `constexpr` — all pure compile-time/size wins with zero runtime cost. See Part A §8.

11. **Open decisions to resolve before writing code** (not answered by this research — these are judgment calls for the project owner): default max-listener-count model (compile-time NTTP array vs. intrusive list, or support both as policies); whether v1 supports only synchronous dispatch or also an ISR-safe queued mode (EnTT's `dispatcher` supports the latter — it's a real feature, but a heavier one, see Part A §9); which exact C++ standard to formally pin in `target_compile_features`.

---

# Part A — C++ Language & Architecture Research

## A1. Type-erased callback storage without `std::function`

The core problem with `std::function`: it type-erases via an internal virtual-dispatch-like mechanism, has a small-buffer-optimization (SBO) branch that falls back to heap allocation for anything larger than ~2 pointers of captured state, and carries an empty/"not callable" state that (in exception-enabled builds) throws `std::bad_function_call`. All of that is machinery Lw-Eh doesn't want.

**Function pointer + `void*` context ("fat function pointer").** The floor of type erasure — this is what C HALs and CMSIS drivers already do:
```cpp
struct callback { void(*fn)(void*, int); void* ctx; };
inline void invoke(callback c, int v) { c.fn(c.ctx, v); }
```
2 pointers, no vtable, no heap, trivially copyable. Downside: caller must hand-write the `void*`-unwrapping trampoline for every distinct function/member they bind.

**Ryazanov "Impossibly Fast" delegate pattern** ([CodeProject, 2005](https://www.codeproject.com/articles/11015/the-impossibly-fast-c-delegates)) — a fully standard-conforming technique: a *templated static stub* generated at the bind call site captures the concrete callable type, but the delegate object itself only ever stores `void*` + a plain non-member function pointer to that stub:
```cpp
template <typename> class delegate;
template <typename Ret, typename... Args>
class delegate<Ret(Args...)> {
  using stub_t = Ret(*)(void*, Args...);
  void*  obj_{};  stub_t stub_{};
  template <auto Fn, typename T>
  static Ret method_stub(void* p, Args... a) { return (static_cast<T*>(p)->*Fn)(a...); }
public:
  template <auto MemFn, typename T> void bind(T* inst) { obj_ = inst; stub_ = &method_stub<MemFn, T>; }
  Ret operator()(Args... a) const { return stub_(obj_, a...); }
};
```
`sizeof(delegate<void(int)>)` = 2 pointers (8 bytes on a 32-bit MCU). No `<functional>` instantiation, no allocator, no throw path. Each `bind<...>()` call instantiates one tiny stub (often <16 bytes at `-Os` on Xtensa/RISC-V), and structurally-identical stubs across call sites are foldable by the linker (`--icf=all`) or COMDAT merging.

**Clugston "Fastest Possible" delegate** ([CUJ/CodeProject 2004](https://www.codeproject.com/articles/Member-Function-Pointers-and-the-Fastest-Possible)) — goes further by exploiting compiler-specific pointer-to-member-function memory layouts via `reinterpret_cast`, sometimes reaching a single-pointer-sized closure with "only two ASM opcodes" of call overhead. Worth studying for how tight the ceiling can get, but it depends on undocumented, compiler/ABI-specific pointer-to-member representation — riskier than Ryazanov's approach for a project that wants to be portable across GCC's Xtensa and RISC-V backends without relying on unspecified behavior.

**Non-owning delegate / `function_ref` model** ([P0792](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0792r5.html), [`llvm::function_ref`](https://llvm.org/doxygen/classllvm_1_1function__ref.html)) — same 2-pointer storage, but explicitly documented as a *reference*: no ownership, no lifetime management, no empty-state handling, trivially copyable. Known implementations store just `void*` + function pointer, and the design explicitly targets staying at 2 words because "some platform ABIs can pass a trivially copyable 2-word type in registers" — directly relevant for Xtensa/RISC-V calling conventions (a 2-pointer delegate can be passed/returned in registers, zero stack traffic). **This is the right mental model for Lw-Eh's internal delegate type**: non-owning, fixed 2-pointer size, no SBO branch, no fallback allocation path.

**Why this matters for the 1-2KB budget:** every one of these approaches avoids the things that make `std::function` expensive — no allocator instantiation, no SBO branch logic, no exception path even latent in the type. The entire "erasure" cost collapses to one small per-binding stub function plus a fixed 2-pointer struct.

## A2. What makes EnTT's `entt::sigh`/`entt::dispatcher` comparatively heavy

**`entt::sigh`**: internally a `std::vector<delegate<Ret(Args...)>>` per signal, with a `sink` type providing a thin connect/disconnect view over that vector. `publish()` iterates **in reverse** specifically so a listener can safely disconnect itself mid-notification without invalidating iteration ([EnTT wiki](https://github.com/skypjack/entt/wiki/Crash-Course:-events,-signals-and-everything-in-between)) — a real design detail worth borrowing (see A9), but it rides on `std::vector`.

**`entt::dispatcher`** (`basic_dispatcher<Allocator>`, [source](https://github.com/skypjack/entt/blob/main/src/entt/signal/dispatcher.hpp)): maintains a `dense_map` keyed by a type identifier → `dispatcher_handler<Type>`, where each handler holds *both* a `sigh` for immediate ("trigger") dispatch *and* a queue (vector) of pending events for deferred "enqueue + `update()`" dispatch, and supports multiple named queues per event type. This is a runtime type-keyed registry: **any code, anywhere, can start listening to any event type at runtime**, looked up by hash. That generality is exactly the "too much" the project explicitly wants to avoid.

**Type indexing**: EnTT does *not* use `std::type_info`/RTTI — it builds its own compile-time `type_info`/`id_type` (historically a sequential "family" counter, now compile-time string hashing of `__PRETTY_FUNCTION__`-style signatures, FNV1a-based) specifically so it still works under `-fno-rtti`. This part is a genuinely good, borrowable idea. `entt::any` (used elsewhere, sometimes pulled in transitively) adds SBO plus a "fake vtable" (a dispatch function pointer implementing copy/move/destroy per erased type).

**What a minimal clone should deliberately leave out:**

| EnTT feature | Why it's heavy | Minimal-clone alternative |
|---|---|---|
| `Allocator` template param threaded everywhere | Every op becomes allocator-aware; dead weight if allocation is always static | No allocator parameter at all |
| `std::vector`-backed listener/event storage | Heap alloc, growth/reallocation code, strong exception-safety machinery | `std::array<delegate,N>` or intrusive list (A7) |
| Runtime type-keyed registry (`dense_map<id_type, handler>`) | Hash map + lookup cost + generality nobody asked for | Each event type gets its own compile-time-bound dispatcher object — no lookup needed at all |
| Deferred enqueue+`update()` event queues | A whole second feature (double-buffered per-type event vectors) for ECS-tick architectures | Omit; synchronous "call listeners now" only |
| `type_info::name()` debug string retention | Long compiler-generated strings kept in `.rodata` per registered type | Drop name retention; type identity only needs to compare equal, never print |
| Group ordering / multi-listener generality bookkeeping | Infrastructure for arbitrary N listeners with arbitrary semantics | Fixed small N, one documented dispatch order |

Keep: the delegate shape (A1) and the RTTI-free compile-time type-id *idea* (though a design where each event type has its own statically-bound dispatcher avoids needing a type id/registry at all — no hashing, no map).

## A3. Avoiding C++ runtime bloat on embedded (`-fno-exceptions`/`-fno-rtti`)

**API design implications**: no `throw` in the public API — use return codes / `enum class` results / `bool` / assert-and-abort for programmer-error conditions (e.g. exceeding a fixed listener capacity). No `dynamic_cast`, `typeid`, `catch`. GCC's own guidance is to pair `-fno-exceptions` with `-fno-unwind-tables` so `throw` genuinely lowers to `abort()` rather than leaving unwind metadata behind ([bare-metal C++ guide](https://alex-robenko.gitbook.io/bare_metal_cpp/compiler_output/exceptions)).

**Concrete, directly-sourced numbers** (ESP-IDF's own docs, [stable](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/cplusplus.html)): exceptions **and** RTTI are both **disabled by default** in ESP-IDF already — Lw-Eh doesn't need to fight the toolchain, just not turn them back on. Enabling RTTI "typically increases binary size by **tens of kilobytes**." Including `<iostream>` alone adds **~200 KB**. Against a 1-2KB target, either mistake is fatal, not marginal — this is the single biggest practical risk flagged anywhere in this research (see A10).

**Important nuance**: `-fno-exceptions` alone does not fully clean up libstdc++ containers. `std::vector::at()`, `std::string`, etc. still contain internal `throw`-call-sites (`std::__throw_length_error`, `std::__throw_out_of_range_fmt`, ...) baked into the header template code itself; these get instantiated regardless of the *user's* exception flags and "can very quickly add up to many kilobytes" ([true cost of C++ exceptions](https://mmomtchev.medium.com/the-true-cost-of-c-exceptions-7be7614b5d84), [bare-metal C++ guide](https://alex-robenko.gitbook.io/bare_metal_cpp/compiler_output/exceptions)). **This is the strongest argument for avoiding `std::vector`/`std::string`/`std::map` outright rather than trusting the compiler flag to clean them up** — some projects work around this with linker `--wrap` tricks redirecting the throw helpers to a trap, but that's a build-system-level fix Lw-Eh's headers can't guarantee downstream users apply.

**Fixed-capacity / intrusive replacements:**
```cpp
// Instead of: std::vector<delegate<void(int)>> listeners;
std::array<delegate<void(int)>, MaxListeners> listeners{};
std::size_t count = 0;   // no allocator, no growth, no throw path, size known at link time
```
- No `std::string`/`std::map` needed at all if event identity is a compile-time **type**, not a runtime string/hash key.
- Intrusive linked list (A7) instead of any node-allocating container.
- `-fno-rtti` removes the `type_info` object generation and `typeid`/`dynamic_cast` support code per class — note this does **not** remove virtual functions/vtables themselves (see A4).

## A4. Static/compile-time dispatch vs dynamic dispatch, for code SIZE

**CRTP/templates**: eliminate the per-instance vptr (4-8 bytes saved per object) and the class's vtable (N × pointer-size in `.rodata`), and enable full inlining/devirtualization since the compiler sees the concrete target at compile time. **The risk is template instantiation bloat**: "every instance of a template is a completely separate piece of code generated by [the] compiler" ([dirtyhandscoding, *On C++ Code Bloat*](https://dirtyhandscoding.github.io/posts/on-cpp-code-bloat.html)). If `Signal<EventA>`, `Signal<EventB>`, `Signal<EventC>` each independently instantiate their *entire* attach/detach/dispatch logic, code size grows roughly linearly with event-type count even though ~95% of that logic is identical across types.

**The mitigation — thin template shim over a shared non-template core** — is the single highest-leverage size technique in this whole report:
```cpp
// Non-template engine: compiled ONCE, works on type-erased storage.
struct signal_core {
  void attach(void* obj, void(*stub)(void*, const void*), delegate_slot* storage, std::size_t& n, std::size_t cap);
  void dispatch(const delegate_slot* storage, std::size_t n, const void* arg) const;
};
// Template shim: near-zero code per event type, just forwards with the right cast.
template <typename Event, std::size_t N>
class signal : signal_core {
  delegate_slot slots_[N]; std::size_t count_ = 0;
public:
  void publish(const Event& e) { dispatch(slots_, count_, &e); }   // thin, often inlines to a call
};
```
Cited result: "offloading portions of templated code into a private non-templated base class can result in reduction of overall object files size by literally megabytes" at large scale, with one measured case dropping compile time 2.73s → 0.75s doing this (general template-bloat literature, [dirtyhandscoding](https://dirtyhandscoding.github.io/posts/on-cpp-code-bloat.html)). Since Lw-Eh explicitly wants to support *multiple* event types under one 1-2KB ceiling, this pattern is close to mandatory past a handful of event types.

**vtable-based dynamic dispatch**: cost = one vtable per *class* (shared, doesn't grow with instance count) + one vptr per *instance* + an indirect call per invocation (defeats inlining but is a small, constant, non-duplicated cost). **Wins** exactly where the shared-core pattern above wins: when many event types would otherwise each stamp out a full duplicate instantiation. **Loses** when call-site latency actually matters more than code size (not the case here — the project owner explicitly prioritizes size over compile time and, implicitly, over micro-latency). Important correction: **`-fno-rtti` does not disable virtual functions or vtables** — it only removes `typeid`/`dynamic_cast` support; plain virtual dispatch remains fully available and cheap. A hand-written "vtable" (a plain `struct { void(*invoke)(void*, Args...); }`) costs the same as a compiler-generated one but sidesteps any RTTI-interaction questions and is arguably more legible for a from-scratch project — functionally this *is* just the A1 delegate technique again.

**Where full CRTP-per-type still wins**: if a given firmware image realistically declares very few event types (≤3-5), duplicated-per-type instantiation can be cheaper in absolute bytes than paying for a shared engine's one-time fixed cost plus its indirect-call overhead. The crossover favors the shared-core pattern as soon as event-type count grows past small single digits.

## A5. Header-only single-header library conventions

**Include guards vs `#pragma once`**: `#pragma once` is supported by every compiler in scope here (GCC, Clang — the only two toolchains ESP-IDF/Arduino-ESP32 use) and avoids guard-macro name collisions between libraries, but is technically non-standard with rare symlink/file-identity edge cases ([64.github.io/cpp-faq](https://64.github.io/cpp-faq/include-guards-pragma-once/)). Pragmatic convention: use `#pragma once` as the primary guard, and layer a traditional `#ifndef LWEH_HPP_INCLUDED` guard around it too — pure preprocessor text costs zero compiled bytes, so belt-and-suspenders is free.

**`inline` for header-defined functions**: any *non-template* function defined (not just declared) in the header needs `inline` linkage, or multi-TU inclusion violates ODR. Critically, `inline`/`constexpr` functions get **external linkage that the linker COMDAT-folds** across TUs into one copy; `static`/anonymous-namespace functions get **internal linkage**, meaning every TU keeps its own private copy that the linker cannot merge — more bytes. Prefer `inline`/`constexpr` over `static` at header scope.

**`inline` variables (C++17)**: required for any header-scope non-template global/constant data to avoid either an ODR violation or the pre-C++17 `extern`-in-header/define-in-one-.cpp workaround, which would break the "just include one header" promise — "inline variables eliminate the main obstacle to packaging C++ code as header-only libraries" (general C++17 literature). `static constexpr` class members have been implicitly inline-safe since C++17 too.

**ODR-safety for configuration**: if Lw-Eh offers a "max listener count" knob, express it as a template non-type parameter (bakes into the *type*, so different configurations are different types — no collision) rather than a `#define` that silently changes a shared type's layout depending on include order (a classic single-header-library footgun).

**STB-style implementation-macro convention** (`#define LWEH_IMPLEMENTATION` before exactly one include, [nothings/stb](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt)) — borrow **only if** Lw-Eh ends up with any non-template, non-inline definitions that shouldn't rely on COMDAT folding. If Lw-Eh stays 100% templates + `inline`/`constexpr` (likely, given A4's design), this macro machinery is unnecessary complexity carried over from C (where templates/`inline` linkage don't exist) — decide explicitly rather than cargo-culting it.

**Dependency hygiene**: `#include <cstdint>` is fine; avoid convenience-including `<functional>`, `<vector>`, `<memory>` transitively — see A3/A10's `<iostream>` = +200KB example. Wrap everything in `namespace lweh {}` to avoid symbol collisions in firmware images linking several single-header libraries.

## A6. Prior art for technique inspiration (not dependencies)

- **ETL (Embedded Template Library)** — [github.com/ETLCPP/etl](https://github.com/ETLCPP/etl), [etlcpp.com](https://www.etlcpp.com/) — borrow the whole philosophy: fixed-capacity, heap-never-used, capacity-as-compile-time-template-parameter (`etl::vector<T,N>`, `etl::observer`, `etl::delegate` / `delegate_service.h`) is essentially Lw-Eh's target shape already proven out in a real embedded-focused codebase. Study the API surface, don't include it.
- **Boost.Signals2** — study as the anti-pattern. `std::function`-based slots, a `mutex` per signal by default (the existence of an explicit `dummy_mutex` escape hatch for single-threaded use is itself an admission the mutex is an unconditional default cost, [Boost docs](https://www.boost.org/doc/libs/1_86_0/doc/html/signals2/api_changes.html)), automatic lifetime tracking via `shared_ptr`/`weak_ptr` plus a "garbage collecting lock," group-ordering/combiner machinery for aggregating return values. Lesson: never make thread-safety or lifetime-tracking *unconditional* infrastructure paid by every caller — if Lw-Eh needs concurrency safety at all, make it explicit/opt-in (A9).
- **CMSIS-Driver `SignalEvent` callback pattern** — [ARM CMSIS-Driver docs](https://arm-software.github.io/CMSIS_6/main/Driver/theoryOperation.html) — the plain `typedef void (*ARM_XXX_SignalEvent_t)(uint32_t event)`, registered once at `Initialize()`, invoked from ISR context with a bitmask, is the simplest possible *working, shipped* embedded event pattern. Use it as the "floor" — every byte Lw-Eh spends beyond this should buy something concretely worth it (type safety, multi-listener).
- **Ryazanov's Impossibly Fast Delegates** ([CodeProject](https://www.codeproject.com/articles/11015/the-impossibly-fast-c-delegates)) — borrow the standard-conforming void*+stub-trampoline mechanism directly (A1); this should be the delegate Lw-Eh is built on.
- **Clugston's Fastest Possible Delegates** ([CodeProject](https://www.codeproject.com/articles/Member-Function-Pointers-and-the-Fastest-Possible)) — borrow the *understanding* of member-function-pointer ABI representation as background; don't port the reinterpret_cast tricks given they lean on compiler-specific undefined behavior.
- **`llvm::function_ref` / P0792 `std::function_ref`** ([LLVM doxygen](https://llvm.org/doxygen/classllvm_1_1function__ref.html), [P0792R5](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0792r5.html)) — borrow the contract: non-owning, 2-pointer, trivially copyable, no empty-state overhead, deliberately kept to register-passable size.
- **EnTT's `entt::delegate`** ([source](https://github.com/skypjack/entt/blob/main/src/entt/signal/delegate.hpp)) — borrow specifically the RTTI-free compile-time type-id technique and the modern `template<auto MemberFn>` binding ergonomics; do not borrow `sigh`'s `std::vector` storage or `dispatcher`'s runtime type registry (A2).

## A7. Storage/memory model options for fixed small footprint

**Compile-time-fixed max listener count via NTTP:**
```cpp
template <typename Sig, std::size_t MaxListeners> class signal {
  delegate<Sig> slots_[MaxListeners]; std::size_t count_ = 0;
public:
  bool attach(delegate<Sig> d) { if (count_ >= MaxListeners) return false; slots_[count_++] = d; return true; }
};
```
Pros: contiguous storage (cache/branch-predictor friendly on an MCU), zero indirection to reach any listener, capacity errors are simple bounds checks, whole object has a known `sizeof` at compile time and can live in `.bss`/on the stack — no allocation, ever. Cons: capacity is part of the type (`signal<Sig,4>` ≠ `signal<Sig,8>`), over-provisioning wastes `sizeof(delegate)×unused`, and picking different `MaxListeners` at different call sites for "the same" conceptual signal creates extra template instantiations (interacts with A4).

**Intrusive linked list** (listener embeds its own link pointer, so the dispatcher needs zero separate storage):
```cpp
struct listener_node { listener_node* next = nullptr; virtual void invoke(int) = 0; };
class signal {
  listener_node* head_ = nullptr;
public:
  void attach(listener_node& n) { n.next = head_; head_ = &n; }              // O(1), zero allocation
  void dispatch(int v) { for (auto* n = head_; n; n = n->next) n->invoke(v); }
};
```
Pros: dispatcher-side footprint is a **single pointer** regardless of listener count; unbounded listeners with zero capacity planning; attach is O(1) with no allocation because the "node" is just wherever the listener object already lives. Cons: cost is *shifted onto the listener* (every listener pays for a `next` pointer + invoke stub whether it wants to or not); detach needs either a `prev` pointer (doubly-linked) or an O(n) scan; listener lifetime becomes the caller's responsibility — a classic dangling-node bug class if a listener is destroyed without detaching first ([discussion](https://www.codeofhonor.com/blog/avoiding-game-crashes-related-to-linked-lists/)). Same fundamental tradeoff as Linux kernel `list_head` / Boost.Intrusive: move allocation cost onto the object, not the container.

**Practical recommendation**: static array wins for the typical embedded case (1-4 known listeners wired at compile time) — simplest, smallest dispatch loop, no extra state demanded of listener types. Intrusive list wins when the *dispatcher itself* must be embeddable cheaply in many places (a `signal` member inside dozens of small structs) and/or listener count is genuinely unbounded. Given the stated budget is *code* size, not RAM, and static arrays produce a simpler/smaller dispatch loop (bounded for-loop vs. pointer-chasing + virtual call per node), NTTP-capacity arrays are the better default, with intrusive-list as an optional alternate storage policy.

## A8. C++ standard version choice

Directly checked against current toolchain docs (time-sensitive — re-verify against whichever exact version the project pins):

- **ESP-IDF stable** (`xtensa-esp32-elf-gcc` / `riscv32-esp-elf-gcc`, native `idf.py`/CMake build): current stable docs state ESP-IDF "compiles C++ code using C++26 language standard with GNU extensions (`-std=gnu++26`)" **by default** ([ESP-IDF C++ support, stable](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/cplusplus.html)) — Espressif rides GCC's newest `gnu++2c` dialect forward aggressively each release (the v5.1 line defaulted to `gnu++20`). This should **not** be read as "C++26 is a safe baseline" — it just reflects whatever the bundled GCC's newest GNU dialect happens to be, not that every C++23/26 library feature is meaningfully exercised/stable on-target.
- **Arduino-ESP32 core** (what most hobbyist "circuit device" users actually build under): flags apply `-std=gnu++2a` (C++20) via the core's `cpp_flags`, with `gnu++2b` available as an alternate — i.e. presently pinned around **C++20**, one full standard behind raw ESP-IDF's aggressive default.
- **Two different backends**: classic ESP32/S2/S3 use Xtensa GCC (`xtensa-esp32-elf-gcc`); ESP32-C3/C6/C61 use RISC-V GCC (`riscv32-esp-elf-gcc`). Both are mature in ESP-IDF today, but "the ESP32 toolchain" is really two GCC backends — worth validating any standard-version assumption against both if broad ESP32-family support matters.
- **Recommendation**: target **C++17 as the guaranteed-portable floor** (solid across every ESP-IDF version in support, every Arduino-ESP32 core release, and GCC-ARM-Embedded toolchains for other MCU vendors too), and treat **C++20 as safe to opportunistically use behind feature-test macros** (`#if __cpp_concepts`, etc.) since both toolchains clear that bar today — but don't *require* C++20 as the minimum, since that gratuitously cuts off slightly-older pins or non-Espressif targets. Don't rely on C++23/26 features; they're only the *default*, not a stable floor, and other vendors' toolchains lag ESP-IDF's GCC cadence considerably.
- **What C++17 buys with zero hidden codegen cost**: `if constexpr` (compile-time branch elimination — dead branches are *not present* in the binary, stronger than trusting the optimizer), `inline` variables (A5, enables the header-only promise), fold expressions (compile-time variadic reduction), relaxed `constexpr`/`constexpr` lambdas (more logic that's simply absent from the binary). All compile-time-only — pure ergonomics/size wins, zero runtime cost.
- **What C++20 additionally buys**: `consteval` (belt-and-suspenders guarantee something never becomes runtime code, though a `constexpr` context call is already compile-time-evaluated when possible), `concepts` (cleaner constraint syntax than `enable_if` SFINAE, and can *reduce* accidental bloat by rejecting a wrong instantiation at compile time with a clear error instead of silently generating a broken specialization). Nice-to-have polish, not essential.

Sources: [ESP-IDF v5.1 C++ support](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-guides/cplusplus.html), [ESP-IDF stable C++ support](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/cplusplus.html), `arduino-esp32` `platform.txt`/`cpp_flags` (github.com/espressif/arduino-esp32).

## A9. Interrupt/concurrency considerations (flagged, not solved)

- **ISR-safety of the dispatch call itself**: if `dispatch()` can be called directly from an ISR (common for a HAL-facing event bus — GPIO edge, peripheral-complete), every transitively-invoked listener inherits ISR restrictions: no blocking calls, no non-`...FromISR` FreeRTOS API, and on ESP32 specifically, code must be `IRAM_ATTR`-placed to avoid faulting when the flash cache is disabled (e.g. during a flash write). Lw-Eh's own dispatch-loop code should be small and free of hidden calls into cacheable-flash-only code so users *can* mark it `IRAM_ATTR` reliably.
- **No locks/mutexes in the dispatch path**: taking a mutex inside an ISR is a deadlock/priority-inversion hazard on virtually every RTOS. Any concurrency protection should be lock-free (atomics) or a minimal disable-interrupts critical section (`portENTER_CRITICAL`/`portEXIT_CRITICAL` on FreeRTOS/ESP32) — never an RTOS mutex/semaphore baked into the dispatcher type. Per A6's Boost.Signals2 lesson: stay unopinionated here and let the caller decide, rather than making every user pay for safety-by-default.
- **Reentrancy / mutation-during-iteration**: a listener detaching itself (or another listener) mid-`dispatch()` is a real hazard for both storage models in A7. EnTT's `sigh` iterates in reverse specifically to make self-disconnect-during-publish safe — Lw-Eh should pick and document one explicit contract here (this is a very common real pattern: a one-shot listener that detaches itself).
- **ISR-raise / task-handle hand-off**: if events must be *raised* in an ISR but *handled* in task context rather than calling listeners directly from the ISR, that needs an ISR-safe hand-off (lock-free SPSC ring buffer, or `xQueueSendFromISR`/`xTaskNotifyFromISR`-style primitives). This is a materially different, heavier feature (essentially EnTT's queued-dispatch mode, A2) — treat as an optional layer on top of a synchronous core, not baked into it, both for size and because the right primitive is RTOS/platform-specific.
- **Multi-core**: classic ESP32 is dual-core Xtensa, ESP32-C6 is dual-core RISC-V. Shared dispatcher state (listener storage, any "currently dispatching" flag) needs at minimum `volatile`/`std::atomic` treatment if touched from both cores/ISRs concurrently — flagged as needing an explicit design position, not solved here.

## A10. Realistic size budget breakdown

**Verdict**: 1-2KB is achievable for the shared dispatch mechanism across a modest number of event types, but it is a function of *how the project uses the library*, not a fixed cost of the library itself — it scales with (a) how many distinct event/signature types get instantiated and (b) how much logic is shared (A4's non-template core) vs. duplicated (naive CRTP-per-type).

Best-effort byte breakdown (Xtensa LX6/LX7 and RISC-V RV32IMC are both fairly dense ISAs; figures are `-Os` engineering estimates from first principles, explicitly **not** measured — flagged as such rather than presented as benchmark data):

| Component | Estimated cost | Notes |
|---|---|---|
| Shared delegate/dispatch core (non-template engine) | ~80-200 bytes, one time | Stub-calling convention + bounds-checked attach/detach, if written once and shared (A4) |
| Per distinct event type, thin template shim over shared core | ~20-80 bytes **each** | Mostly forwarding; compiler can often inline/tail-call this close to nothing |
| Per distinct event type, fully independent template (no shared core) | ~150-400 bytes **each** | Attach/detach/dispatch all duplicated — the "naive CRTP-everywhere" cost, why A4 matters once event-type count > 2-3 |
| Per listener registration call site (binding a specific function) | ~10-30 bytes **each** | Generated stub trampoline; foldable via `--icf=all`/COMDAT if structurally identical |
| Dispatch loop itself (static-array model) | ~20-50 bytes, one time per signature | Bounded for-loop + indirect call, renders very tightly at `-Os` |
| RTTI accidentally left enabled (one polymorphic class slips through) | +tens of bytes to **+tens of KB** | Per ESP-IDF's own docs; catastrophic relative to budget |
| Transitively including `<functional>`/`<vector>`/`<iostream>` | +hundreds of bytes to **+200KB** | `<iostream>` alone ≈ 200KB on ESP-IDF — the most likely way this budget silently blows up |

**Direct answer to "is 1-2KB realistic, or is that one trivial event type?"**:
- **A single trivial event type** (one signature, a handful of static listener slots, no shared-core machinery needed): 1-2KB is comfortable, realistically closer to **200-500 bytes**, with headroom to spare.
- **A general type-safe multi-event-type dispatcher** meant for reuse across many event payloads in one firmware image: 1-2KB is achievable **only if** the shared non-template engine / thin-shim pattern (A4) is adopted from the start — cost then grows by tens of bytes per additional event type rather than hundreds, and a realistic image with 5-15 event types could plausibly land in the 1-2KB range for the library's total contribution. Under a naive fully-templated-per-type design, 1-2KB likely covers only **2-5 event types** before the budget is exhausted — this is a linear-growth problem, not a one-time cost, and the project owner should treat the target as "1-2KB for the shared mechanism, plus small marginal per-event-type cost," not "1-2KB total no matter how many event types get declared."
- **The biggest risk to the budget is not the dispatcher logic** — it's transitively pulling in any piece of "normal" STL even once, anywhere, even incidentally (a convenience type alias, a macro-gated debug path that still gets parsed/instantiated). At this budget size, an accidental `#include <functional>` is an existential risk, not a rounding error.
- **Recommended acceptance test**: don't trust any a priori estimate (including this one) — build the smallest representative example (`-Os -flto -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections`, linked for a real ESP32 target) early, and read the actual `.text`+`.rodata` delta over an otherwise-empty firmware image via `idf.py size`/`nm --size-sort`. Template-heavy C++ code size is notoriously non-obvious from source alone; a measured number should anchor the target, not an estimate.

---

# Part B — Build System & Compiler/Linker Toolchain Research

## B1. CMake project structure for a header-only, zero-dependency library

**Core pattern: `INTERFACE` library target.** An `INTERFACE` library has no compiled sources of its own — it exists purely to propagate *usage requirements* (include paths, compile definitions, compile features, compile/link options) to whatever target consumes it via `target_link_libraries`.

```cmake
add_library(lw_eh INTERFACE)
add_library(LwEh::lw_eh ALIAS lw_eh)   # namespaced alias, works identically pre/post install
target_include_directories(lw_eh INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(lw_eh INTERFACE cxx_std_17)
```

- Use `$<BUILD_INTERFACE:...>` / `$<INSTALL_INTERFACE:...>` generator expressions so the same target works both from the build tree (submodule/`add_subdirectory`) and from an installed/exported package, without leaking an absolute build-tree path into installed config files.
- **Modern alternative (CMake ≥3.23): `target_sources(... FILE_SET HEADERS)`.** File sets let you declare public headers explicitly (`BASE_DIRS`, `FILES`) rather than relying purely on include-dir propagation, and `install(TARGETS ... FILE_SET HEADERS)` installs exactly the declared headers with correct relative layout. This is the CMake-blessed replacement for manually gluing `target_include_directories` + `install(DIRECTORY include/ ...)`. ([cmake.org](https://cmake.org/cmake/help/latest/command/target_sources.html))
- Never attach `INTERFACE_SOURCES` (compiled `.cpp`) to this target unless you deliberately want a compiled component — for a pure header-only library, zero sources means zero possibility of ODR/ABI mismatch between what the library "is" and what gets compiled into the consumer.
- Do **not** set `INTERFACE_LINK_LIBRARIES` to anything — an empty link-libraries set is itself a proof artifact that the target adds no dependency edges (see B6).

**Making the build provably hermetic/offline-safe — an explicit avoid-list:**

| Mechanism | Why it breaks hermeticity | Rule for Lw-Eh |
|---|---|---|
| `FetchContent_Declare` / `FetchContent_MakeAvailable` | Runs at **configure time**, does a network `git clone`/`URL` download and `add_subdirectory`'s the result in-tree | Must not appear anywhere in the repo's CMake files |
| `ExternalProject_Add` | Runs at **build time** as a sub-build with download→configure→build→install steps (`GIT_REPOSITORY`/`URL` args); more insidious than FetchContent since it can fetch mid-build with less up-front visibility | Must not appear anywhere |
| `find_package(Foo)` (MODULE mode) | Falls back to CMake's bundled `FindFoo.cmake`, which searches system paths / shells to `pkg-config` — result depends on whatever happens to be installed on the build machine (non-deterministic, not "fetched" but still non-hermetic) | Zero `find_package` calls anywhere in Lw-Eh's own CMakeLists |
| `find_package(Foo CONFIG)` | Searches `CMAKE_PREFIX_PATH`, the per-user package registry (`~/.cmake/packages`), and any prefixes injected by an ambient vcpkg/Conan toolchain file | Same — zero calls |
| vcpkg (`-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake`) | Hooks `find_package`/`find_library` to resolve against vcpkg's installed tree; vcpkg's own install step is a network fetch+build | Lw-Eh never calls `find_package`, so there is nothing for an *ambient* vcpkg toolchain file (set for unrelated reasons by a consumer) to hook on Lw-Eh's behalf |
| Conan (`conan_toolchain.cmake` / `conanbuildinfo.cmake`) | Same class of risk via `find_package(Foo REQUIRED)` / `conan_basic_setup()` | Same guarantee: no `find_package`, no `conanfile.txt`/`conanfile.py`/`vcpkg.json` manifest committed to the repo (their mere presence invites tooling auto-activation) |

**Proving it, not just asserting it:**
- CI job that runs `cmake` configure+build with network access disabled (firewalled runner, no NIC in a container) — if it still succeeds, that's your hermeticity proof.
- A cheap lint/CI grep gate: `grep -RIn -E "FetchContent|ExternalProject_Add|find_package|find_program|find_library" CMakeLists.txt cmake/ **/CMakeLists.txt` failing the build on any match (belt-and-suspenders against future contributions).
- The single top-level `find_package`-free, `FetchContent`-free `CMakeLists.txt` is itself the hermeticity contract — there's no CMake "offline mode" flag to lean on instead; the guarantee comes from what commands are simply never invoked. ([cmake.org – FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html))

## B2. CMake toolchain files for cross-compiling to embedded targets

A CMake **toolchain file** is a small `.cmake` script passed via `-DCMAKE_TOOLCHAIN_FILE=path/to/file.cmake` at the *first* configure invocation. For anything without a hosted OS, the universal skeleton is:

```cmake
set(CMAKE_SYSTEM_NAME Generic)              # "no OS" — required for baremetal
set(CMAKE_SYSTEM_PROCESSOR <arch>)          # informational + used by some generators
set(CMAKE_C_COMPILER   <prefix>-gcc)
set(CMAKE_CXX_COMPILER <prefix>-g++)
set(CMAKE_ASM_COMPILER <prefix>-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)   # see note below
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

- `CMAKE_SYSTEM_NAME Generic` is the documented signal for "embedded target without an OS"; it disables assumptions CMake otherwise makes about a hosted environment.
- `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` is needed because CMake's internal compiler-sanity `try_compile` defaults to linking a full executable, which fails on a bare-metal toolchain with no default linker script/entry point/libc semantics until your real linker script is involved; forcing a static-lib try-compile sidesteps that.
- **Anti-pattern to avoid**: setting `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS` directly inside the toolchain file. This was flagged as a real problem in ESP-IDF's own CMake integration — it clobbers/fights user- or cache-supplied flags. The CMake-correct mechanism (CMake ≥3.21) is `CMAKE_<LANG>_FLAGS_INIT`, which *seeds* the initial cache value without permanently overriding user overrides. ([github.com/espressif/esp-idf#7507](https://github.com/espressif/esp-idf/issues/7507))

**ESP32 — Xtensa variants (esp32, esp32s2, esp32s3):**
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)
set(CMAKE_C_COMPILER   xtensa-esp32-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp32-elf-g++)
```
Swap `xtensa-esp32-elf-*` → `xtensa-esp32s2-elf-*` / `xtensa-esp32s3-elf-*` per chip. **Naming currency note:** Espressif has been consolidating the per-chip Xtensa toolchains into a single unified `xtensa-esp-elf-gcc`/`xtensa-esp-elf-g++` build (chip selected via `-mcpu=`/config rather than a separate install per chip) starting around the crosstool-NG `esp-14.x` generation; you'll see both `xtensa-esp32s3-elf-gcc`-style and unified `xtensa-esp-elf-gcc` references depending on ESP-IDF version — both are legitimate, check what your target ESP-IDF release ships. ([github.com/espressif/esp-idf#18172](https://github.com/espressif/esp-idf/issues/18172))

**ESP32 — RISC-V variants (esp32c3, esp32c6, and h2/c2/c5/c61):**
```cmake
set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_C_COMPILER   riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
```
`riscv32-esp-elf-gcc` is already the single unified name across the RISC-V ESP32 family (no per-chip variant naming the way legacy Xtensa had) — version-matching between this toolchain and the ESP-IDF release matters (mismatch produces a CMake warning). ([esp32.com/viewtopic.php?t=21108](https://esp32.com/viewtopic.php?t=21108))

**Reference pattern: ARM Cortex-M (`arm-none-eabi-gcc`):**
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
```
Reference implementations: `jobroe/cmake-arm-embedded`, `umanovskis/baremetal-arm`. ([github.com/jobroe/cmake-arm-embedded](https://github.com/jobroe/cmake-arm-embedded))

**Reference pattern: AVR (`avr-gcc`):**
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_C_COMPILER   avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)
set(MCU "atmega328p" CACHE STRING "target AVR device")
set(CMAKE_C_FLAGS_INIT   "-mmcu=${MCU}")
set(CMAKE_CXX_FLAGS_INIT "-mmcu=${MCU}")
```
`-mmcu=<device>` is mandatory on AVR (selects exact chip + default linker script). AVR uses its own minimal **avr-libc**, not newlib/picolibc — a materially different C-library lineage from the ARM/Xtensa toolchains, worth remembering when reasoning about B8. Reference: `mkleemann/cmake-avr`, `nnarain/cmake-avr-toolchain`. ([nnarain.github.io](https://nnarain.github.io/2016/03/29/AVR-CMake-Toolchain.html))

Since Lw-Eh itself is `INTERFACE`-only and toolchain-agnostic, **it needs none of these toolchain files itself** — they're artifacts the *consumer's* firmware project supplies. Lw-Eh's own repo only needs one (or a handful, for CI matrix testing) as a convenience/CI aid, not as part of the library's shipped contract.

## B3. Practical integration with ESP-IDF's CMake build system

ESP-IDF wraps raw CMake with its own component model. A component with **zero dependencies and only headers** is the simplest possible component:

```
components/lw_eh/
├── CMakeLists.txt
└── include/
    └── lw_eh/
        └── dispatcher.hpp   (+ other headers)
```

```cmake
# components/lw_eh/CMakeLists.txt
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES ""          # explicit empty — self-documents zero deps
)
```

- `idf_component_register(INCLUDE_DIRS "include" ...)` with no `SRCS` is the documented shape for header/Kconfig-only components — the include directory is exposed to consumers, no library object is compiled from Lw-Eh's own component. ([docs.espressif.com – Build System](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html))
- Leave `REQUIRES`/`PRIV_REQUIRES` empty — this is the ESP-IDF-native expression of "zero dependencies."
- **Don't fight ESP-IDF's global optimization config.** A real ESP-IDF project already drives `-Os`/`-O2`/LTO project-wide via `CONFIG_COMPILER_OPTIMIZATION_*` Kconfig options (menuconfig / `sdkconfig`) rather than per-target `CMAKE_BUILD_TYPE` — ESP-IDF's own build system largely ignores `CMAKE_BUILD_TYPE` in favor of these Kconfig-driven flags ([github.com/esp-rs/esp-idf-template#162](https://github.com/esp-rs/esp-idf-template/issues/162)). So the component's `CMakeLists.txt` should generally **not** redundantly push its own `-Os`/`-flto` via `target_compile_options` here — that aggressive flag set (B10) belongs in Lw-Eh's *standalone* `CMakeLists.txt` for non-ESP-IDF consumers; inside ESP-IDF, defer to the project's own sdkconfig.
- **Distribution channel caveat**: the modern [ESP Component Registry](https://components.espressif.com) (`idf_component.yml` manifest, `idf.py add-dependency`) *does* fetch over the network — but that fetch is initiated by the **consumer's** build (their `idf_component.yml` referencing Lw-Eh), not by Lw-Eh's own `CMakeLists.txt`. This is consistent with the hard constraint ("Lw-Eh's own build never fetches") but worth flagging explicitly: publishing to the registry is a legitimate optional distribution channel alongside plain git-submodule vendoring, not a violation, as long as Lw-Eh itself never lists a dependency there.
- No toolchain file is needed at the component level — the enclosing ESP-IDF **project** already supplies `idf.py`-managed `toolchain-esp32*.cmake` files internally.

**PlatformIO / Arduino-ESP32 shape (when meaningfully different):**
- **`framework = espidf`** under PlatformIO drives the real ESP-IDF CMake build underneath — the B3 component recipe above applies verbatim; PlatformIO just orchestrates `idf.py`.
- **`framework = arduino`** (classic Arduino/PlatformIO Library Dependency Finder) does **not** use CMake at all on the consumption side — it's PlatformIO's SCons-based builder. The only artifact needed is a metadata manifest, not a second build system:
  ```json
  // library.json
  { "name": "lw_eh", "version": "0.1.0",
    "frameworks": ["arduino", "espidf"], "platforms": "*",
    "build": { "includeDir": "include" } }
  ```
  Header-only means no `src/*.cpp` at all — just headers under `include/` (or PlatformIO's simpler convention of headers directly under `src/`), referenced by the manifest so PlatformIO's Library Dependency Finder adds the include path. Legacy Arduino-IDE-style libraries would instead ship `library.properties`, but `library.json` is the more capable/recommended manifest. ([docs.platformio.org – Creating Library](https://docs.platformio.org/en/latest/librarymanager/creating.html))
- **`framework = arduino, espidf`** (combined): Arduino is pulled in as a component of an ESP-IDF build, so the underlying build is CMake either way — again, the B3 recipe applies.
- Bottom line: CMake is the one real build description; `library.json` is a thin manifest shim for the non-ESP-IDF Arduino/PlatformIO audience, not a parallel build system to maintain.

## B4. Exhaustive compiler flag list for smallest possible code size

**Framing note that shapes everything below**: at a 1-2 KB *total* target, the usual embedded-size-optimization discourse (framed around shaving tens of KB off multi-hundred-KB firmware) doesn't scale down cleanly — a single `.comment` section (`-fno-ident`), one stray `typeinfo` object (RTTI), or one unnecessary `.eh_frame` entry is a proportionally large bite out of the budget. Every flag below is worth taking seriously at this scale, not just the "obvious" ones.

**Optimization level:**

| Flag | Compiler | Effect |
|---|---|---|
| `-Os` | GCC & Clang | Optimize for size: applies most `-O2` optimizations except those that tend to increase code size. |
| `-Oz` | **Clang only** | More aggressive than `-Os` — trades additional performance for further size reduction. GCC has **no equivalent flag**; `-Os` is GCC's smallest level. ([interrupt.memfault.com](https://interrupt.memfault.com/blog/arm-cortexm-with-llvm-clang)) |

**Whole-program / LTO:**

| Flag | Compiler | Effect |
|---|---|---|
| `-flto` | GCC & Clang | Enables link-time optimization; GCC emits GIMPLE bytecode into objects (see B5 fat vs. slim), Clang defaults to **full LTO** (whole program merged into one IR module at link time). |
| `-flto=thin` | Clang only | ThinLTO — scalable/parallel, imports per-TU summaries instead of merging everything; faster and more scalable but *slightly* less thorough than full LTO. Since compile time is explicitly not a constraint here, prefer plain `-flto` (full) over `-flto=thin` for maximum size squeeze. |
| `-flto=N` / `-flto=auto` | **GCC only** | ⚠️ Not an optimization-thoroughness knob like Clang's thin/full — this controls **parallel LTO job count** (N jobs, or auto-detect). Don't confuse with Clang's `thin`/`full` semantics. |
| `-flto-partition=none` | GCC only | Forces a single non-partitioned WPA unit instead of GCC's default multi-partition LTO — closest GCC analog to Clang's "full" LTO; maximizes cross-module optimization at the cost of link-time memory/time (irrelevant per project constraints). |
| `-ffat-lto-objects` | GCC only | Object files carry *both* GIMPLE bytecode and normal machine code (larger `.o`/`.a` files, but usable with non-LTO-aware tooling / mixed LTO+non-LTO linking). Generally **not needed** for Lw-Eh itself since it's header-only (no `.a` ever produced); relevant only if Lw-Eh ever ships a compiled test/example archive. ([gcc.gnu.org – LTO Overview](https://gcc.gnu.org/onlinedocs/gccint/LTO-Overview.html)) |

**Dead-code elimination enablers:**

| Flag | Effect |
|---|---|
| `-ffunction-sections -fdata-sections` | Places each function/global variable in its own ELF section (`.text.foo`, `.data.bar`) instead of one monolithic `.text`/`.data`, enabling the linker to discard unreferenced ones individually (paired with `--gc-sections`, below). |

**C++ runtime feature stripping:**

| Flag | Effect |
|---|---|
| `-fno-exceptions` | Disables C++ exception support — removes exception tables, landing pads, personality-routine references (`__gxx_personality_v0`); `throw` becomes ill-formed/aborts. Removes reliance on the unwinder (`_Unwind_*`, `__cxa_throw`) which can cost real KB even if never thrown. |
| `-fno-rtti` | Disables `dynamic_cast` (to polymorphic types) and `typeid` — removes `type_info` objects and their name strings plus the RTTI comparison runtime. |
| `-fno-unwind-tables` | Omits `.eh_frame` tables used for non-exception stack unwinding (debugger backtraces). |
| `-fno-asynchronous-unwind-tables` | Stops emission of unwind info valid at *every* instruction boundary (needed for async-signal unwinding). This is the bigger generator on x86-64 (on by default there!); usually already off on typical bare-metal ARM/AVR/Xtensa GCC, but explicit is cheap insurance and matters if you ever build host-side x86-64 tests. |
| `-fno-threadsafe-statics` | Disables the C++11-mandated thread-safe guard-variable dance (`__cxa_guard_acquire`/`release`) around function-local `static` init. Safe on single-threaded MCU firmware. |
| `-fno-use-cxa-atexit` | Switches static-destructor registration from `__cxa_atexit` to plain `atexit()`. Needs `HAVE_INITFINI_ARRAY_SUPPORT` in the libc to route destructors into `.fini_array` correctly on newlib-nano. |

**Codegen shape:**

| Flag | Effect |
|---|---|
| `-fomit-frame-pointer` | Drops the dedicated frame-pointer register, freeing a register (meaningful on register-starved ISAs) and shrinking prologues/epilogues; often default at `-O1`+ already, but explicit is safe. Trade-off: harder stack backtraces without one (acceptable for release firmware). |
| `-fmerge-all-constants` | Merges identical `const`/`constexpr` values in `.rodata` (not just string literals, which merge by default) — technically permits merging named constants that strictly shouldn't alias, in exchange for dedup. |
| `-fno-stack-protector` | Removes stack-smashing canary checks (`__stack_chk_guard`/`__stack_chk_fail`) some embedded GCC configs enable by default. **Security trade-off, not a free lunch** — a conscious choice, lower-risk for a pure dispatch library with no untrusted-buffer parsing. |
| `-ffreestanding` | Declares the environment doesn't guarantee hosted standard-library semantics; implies `-fno-builtin`. |
| `-fno-builtin` | Stops the compiler substituting known-library-function calls with builtin equivalents. **Double-edged for size** (can shrink *or* grow code depending on the call site) — generally leave this **off** unless writing a from-scratch runtime with non-standard libc semantics; not a blanket win. |
| `-fvisibility=hidden` (+ `-fvisibility-inlines-hidden` for C++) | Defaults ELF symbol visibility to hidden. Its classic win (shrinking `.dynsym`) doesn't apply to a statically-linked MCU image with no shared objects, but it still lets the compiler assume no external interposition (slightly better codegen) and keeps `.symtab` tidy — low-risk, real, just smaller win here than on desktop/shared-lib targets. |
| `-fno-ident` | Suppresses the `.comment` section (compiler name+version string) — a handful of bytes, but 100% waste in a shipped binary. |
| `-fshort-enums` | Lets the compiler pick the smallest integer type that fits an enum's values instead of always `int`. **ABI-breaking flag** — changes `sizeof(enum)`/struct layout; every TU sharing enum-containing structs across a link boundary must agree. AVR-GCC and some ARM EABI variants default this on already. Safer library-design alternative: use `enum class Foo : uint8_t` explicitly in Lw-Eh's public API instead of depending on this flag. |

**Architecture-specific flags worth knowing:**

| Flag | Architecture | Effect |
|---|---|---|
| `-mthumb` | ARM Cortex-M | Emits Thumb-2 encoding instead of 32-bit ARM — Thumb-2 is *significantly* denser; virtually mandatory for Cortex-M size work. |
| `-mcpu=cortex-mN` / `-mfloat-abi=` / `-mfpu=` | ARM Cortex-M | Selects exact core/FPU config — wrong config either misses size-relevant ISA features or (worse) miscompiles. |
| `-mlongcalls` | Xtensa (ESP32) | Assembler emits `L32R`+`CALLX` instead of a direct `CALL` for potentially out-of-range targets — needed once code exceeds direct-call range; ESP-IDF enables it by default. |
| `-mtext-section-literals` | Xtensa (ESP32) | Interleaves literal pools into `.text` next to their referencing function instead of a separate literal section — trade-off between locality and cross-TU literal-pool dedup by the linker (default is the *separate*-section behavior, which usually wins on size due to redundant-literal elimination). ([gcc.gnu.org – Xtensa Options](https://gcc.gnu.org/onlinedocs/gcc/Xtensa-Options.html)) |
| `-march=rv32imc` / `-mabi=ilp32` | RISC-V (esp32c3/c6) | The `C` (compressed instructions) extension is the RISC-V analog to ARM Thumb — essential for size; omitting it costs significant density. |
| `-mmcu=<device>` | AVR | Mandatory device selection; also picks the correct default linker script. |
| `-mstrict-align` | AArch64/ARM | Forces aligned-only memory access codegen (relevant if target hardware traps unaligned access) — a **correctness**, not primarily a size, flag; note documented GCC/LLVM bugs where it doesn't catch every case at `-O2`/`-O3` ([github.com/llvm/llvm-project#95811](https://github.com/llvm/llvm-project/issues/95811)). Less relevant to Cortex-M (mostly an AArch64/ARMv8-A concern) — include for completeness since "circuit devices like ESP32 and such" spans architectures with different alignment-fault behavior. |
| `-fno-jump-tables` | GCC/Clang, any arch | Prevents jump-table generation for `switch`; can shrink `.rodata` at a runtime-speed cost — lesser-known lever, situational. |

**Companion linker flags:**

| Flag | Effect |
|---|---|
| `-Wl,--gc-sections` | Discards `-ffunction-sections`/`-fdata-sections` sections with no live references — the essential companion to the compile-time flag above. |
| `-s` / `-Wl,--strip-all` | Strips all symbol table and relocation info from the linked output. Keep an unstripped copy for debugging; strip only the flashed artifact (e.g. via `objcopy --strip-all` when producing the final `.bin`). |
| `-Wl,-Map=out.map` | Emits a full linker map (symbol addresses, section placement, which object pulled in which symbol) — the primary size-audit artifact, see B9. |
| `--icf=all` (LLD / gold / mold) | True identical-code-folding at the linker level — merges bit-identical function bodies **regardless of name/type**, catching cases plain COMDAT-by-name folding misses. **Caveat**: plain **GNU ld (bfd)** — the default linker in most bare-metal cross toolchains (`arm-none-eabi`, `avr-gcc`, `xtensa-esp32-elf`) — implements **no `--icf` at all**. Only gold, LLD, and mold support it, and of those only LLD has meaningfully growing bare-metal/embedded-target support; gold and mold are essentially Linux/host-ELF-focused. So on Xtensa/AVR toolchains today, linker-level ICF usually isn't available — GCC's own `-fipa-icf` (default on at `-O2`/`-Os`, see B5) is the portable ICF mechanism across these targets since it's a compiler pass, not a linker feature. ([maskray.me – linker options explainer](https://maskray.me/blog/2020-11-15-explain-gnu-linker-options)) |
| `-Wl,--relax` | Post-hoc shrinks conservatively-long instruction sequences once final addresses are known (e.g. long call/load-address sequences → short forms when in range). Biggest and most commonly cited impact on **RISC-V** (shrinks `AUIPC+JALR` to compressed `C.J`/`JAL`, GP-relative relaxation), also present for Xtensa `ld` (relaxes `L32R`/long-call sequences) and traditionally AVR/MSP430. ARM/Thumb-2 `ld` does comparatively little relaxation since Thumb-2 encoding is largely fixed at assemble time. |
| `-Wl,--print-gc-sections` | Diagnostic companion to `--gc-sections` — prints exactly which sections were discarded; useful when *verifying* dead-code elimination actually fired (see B9). |

## B5. Whole-program/link-time optimization specifics

**Does LTO matter for header-only, template-heavy code?** Two different scenarios need separating:

1. **Lw-Eh's own code, viewed in isolation.** When a consumer's `.cpp` TU `#include`s Lw-Eh and instantiates its templates, the compiler already has full visibility of all of Lw-Eh's inline/template code *within that TU* at ordinary `-O2`/`-Os` compile time — inlining, dead-code elimination, and constant propagation across Lw-Eh's own code already happen without any LTO involved. This is the actual optimization value proposition of "header-only": **no LTO is required for intra-TU inlining of the library's own code.**

2. **Where LTO's marginal value actually shows up:**
   - **Cross-TU template-instantiation dedup.** If the consumer instantiates, say, `dispatcher<MyEvent>` identically in two different `.cpp` files, each TU emits its own weak/COMDAT (`.gnu.linkonce`-style) copy. **Ordinary linking already deduplicates *exact* duplicate COMDAT groups by symbol name** — that's a basic linker feature, not something requiring LTO. What LTO/ICF specifically adds beyond that is folding *different*-named instantiations that happen to compile to bit-identical machine code (e.g., `dispatcher<EventA>` and `dispatcher<EventB>` where `EventA`/`EventB` share layout) — plain name-keyed COMDAT folding can't catch this; only true ICF (GCC's `-fipa-icf`, or a linker's `--icf=all`) can.
   - **Optimizing the *boundary* between Lw-Eh and the rest of a multi-TU application** — e.g., inlining a user's own event-handler callback (defined in `handlers.cpp`) into Lw-Eh's dispatch call site (instantiated from a header, physically emitted in `main.cpp`) requires whole-program visibility that no single TU has. This is a property of the **consumer's overall build**, not something Lw-Eh's own `CMakeLists.txt` can force — Lw-Eh can only make sure it doesn't get in the way (visibility flags, no artificial compilation barriers).
   - **Net guidance**: recommend the consumer enable LTO for their *whole firmware image* (general good practice regardless of Lw-Eh), but don't oversell it as something that shrinks Lw-Eh's own contribution specifically — for a strictly header-only, single-instantiation-pattern library, most of LTO's usual multi-TU payoff already happened for free at ordinary compile time.

**Known caveats/maturity issues with LTO on the ESP32 Xtensa GCC toolchain specifically:**
- Espressif's own official "Minimizing Binary Size" guide **does not mention LTO at all** — no flags, no caveats, no recommendation either way, as of the current stable docs. ([docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/size.html)) That silence is itself a signal about how battle-tested/front-and-center LTO is in Espressif's own size guidance, despite `CONFIG_COMPILER_LTO` existing as an opt-in Kconfig knob in `idf.py menuconfig` (its default has changed across ESP-IDF releases — verify against the specific IDF version rather than assuming).
- **"dangerous relocation: call8/call0: call target out of range"** is a real, recurring class of failure on Xtensa when binaries grow large (documented at ~500KB+ code size in some reports), tied to Xtensa's limited-range call/literal-load encodings. It shows up even with `-mlongcalls` set in some cryptography-heavy dependency cases. ([github.com/esp-rs/esp-idf-hal#392](https://github.com/esp-rs/esp-idf-hal/issues/392), [github.com/briansmith/ring#1989](https://github.com/briansmith/ring/issues/1989)) LTO *exacerbates the risk* rather than uniquely causing it — cross-TU inlining grows individual functions/regions, and merging translation units reduces the number of call sites the assembler could otherwise resolve with a short direct encoding. **This class of issue is essentially irrelevant to Lw-Eh's own 1-2 KB contribution** — it only matters for the consumer's *overall* firmware image if LTO'd as a whole and it's large; worth a documentation footnote for Lw-Eh users doing whole-program LTO, not a reason for Lw-Eh itself to avoid anything.
- Xtensa is a vendor/community-maintained GCC backend (crosstool-NG-built, not a GCC-mainline-tier target like x86/ARM), so LTO maturity there has historically lagged the flagship backends — treat whole-program LTO on Xtensa as something to validate empirically per toolchain version rather than assume is bulletproof.
- **Fat vs. slim LTO objects** matter if Lw-Eh (or a consumer) ever archives compiled test/example code into a `.a`: slim (GCC default) objects contain only GIMPLE and need LTO-aware `ar`/`nm`/`ranlib` (the `gcc-ar`/`gcc-nm`/`gcc-ranlib` wrappers); `-ffat-lto-objects` keeps real machine code alongside GIMPLE for tooling compatibility at the cost of larger intermediate `.o`/`.a` files. Not relevant to Lw-Eh's own header-only distribution (no `.a` ever produced), but relevant if the project ships a compiled test suite. ([gcc.gnu.org/onlinedocs/gccint/LTO-Overview.html](https://gcc.gnu.org/onlinedocs/gccint/LTO-Overview.html))

## B6. What "no linking overhead" concretely means here

Two genuinely separate claims need to be kept distinct:

**(A) "Our library adds no link dependency" — fully achievable, and here's exactly what makes it true:**
- Because `lw_eh` is `add_library(lw_eh INTERFACE)` with **no `INTERFACE_SOURCES`**, there is no `liblw_eh.a` (or `.so`) ever produced. `target_link_libraries(app PRIVATE lw_eh)` on an `INTERFACE` library only propagates **usage requirements** (`INTERFACE_INCLUDE_DIRECTORIES`, `INTERFACE_COMPILE_DEFINITIONS`, `INTERFACE_COMPILE_FEATURES`, `INTERFACE_COMPILE_OPTIONS`) into `app`'s own compile steps — it adds **zero entries** to the actual link command line.
- Concretely, this means: no possibility of an unused-but-linked-in archive member, no risk of ODR duplicate-symbol conflicts between a prebuilt Lw-Eh archive and the consumer's own template instantiations (there is no prebuilt archive), and no separate library search/resolution step for the linker to perform at all.
- **Design discipline that keeps this true**: Lw-Eh's headers must avoid introducing *new* runtime-support dependencies the rest of the firmware wasn't already going to need — specifically, avoid 64-bit integer divide/modulo, avoid floating point in the hot dispatch path, avoid anything that would newly pull in `libgcc.a` helper routines (`__udivmoddi4`, `__mulsi3`, soft-float emulation) that the rest of the firmware wasn't already linking. If Lw-Eh sticks to 32-bit-native integer operations and no FP, it introduces **zero new libgcc dependencies** beyond whatever the rest of the firmware already required.
- Don't propagate `INTERFACE_LINK_LIBRARIES` to anything — an empty link-libraries set is a literal, checkable proof (`get_target_property(x lw_eh INTERFACE_LINK_LIBRARIES)` should be `NOTFOUND`/empty) that the target adds no dependency edges.

**(B) "The platform toolchain always links a small runtime regardless" — true, unavoidable, and orthogonal to Lw-Eh:**

Any bootable C/C++ program on an MCU unavoidably links, at minimum:
- A **startup/crt0 file** (reset handler, `.data` copy-down, `.bss` zeroing, vector table) — CMSIS startup `.s` for ARM, `crt1.o`-equivalent for AVR, ESP-IDF's own app-startup component for ESP32.
- **`libgcc`** — the compiler's own runtime-support routines (soft multiply/divide/float emulation) needed by *any* C/C++ program that uses operations the target ISA doesn't do natively.
- **A minimal libc** — newlib/picolibc (ARM/Xtensa) or avr-libc (AVR) — even a `main()` that does nothing still needs `_exit`/memory-init stubs.

This runtime cost exists **whether or not Lw-Eh is used at all** — it's a property of "compiling any C/C++ program for this target," not something attributable to Lw-Eh. The correct way to communicate this to users: Lw-Eh's contribution to the link is *purely additive-or-nothing* (either the consumer's own code calls into something that changes codegen, or it doesn't), while the platform runtime floor exists regardless and should never be counted against Lw-Eh's 1-2 KB budget when measuring (see B9 for how to isolate the two — diff a build *with* Lw-Eh against an otherwise-identical build *without* it, rather than reading Lw-Eh's size off the absolute firmware total).

## B7. GCC vs Clang for code size on embedded ARM/Xtensa targets

**ARM Cortex-M:**
- Modern (2020s-era) consensus across independent benchmarks: GCC and Clang produce **roughly comparable** code size on Cortex-M, generally within a few percent of each other, with no consistently dominant winner — "In the 2020s, GCC, Clang, and IAR are all rock-solid on Cortex-M. Code generated by different compilers performs almost identically." ([m0agx.eu](https://m0agx.eu/practical-comparison-of-ARM-compilers.html))
- Memfault's own FreeRTOS/NRF52840 comparison: Clang build 25,510 bytes total vs GCC build 25,286 bytes — "within about 1% of each other" for that sample app; use `-Oz` for Clang, `-Os` for GCC. Historically (circa 2013) Clang's ARM size-optimization backend was noticeably behind GCC's; that gap has closed substantially. ([interrupt.memfault.com](https://interrupt.memfault.com/blog/arm-cortexm-with-llvm-clang))
- IAR (commercial) still reported as edging out both open compilers by a small margin (~2-3%) in some comparisons — relevant only if the project ever considers commercial toolchains, which is out of scope given the zero-cost/GCC-toolchain-centric constraint here.
- Some data points show ThinLTO (Clang) underperforming GCC's linker on footprint/runtime in specific cases — another argument for preferring full LTO over ThinLTO when compile time doesn't matter (per B5). ([discourse.llvm.org](https://discourse.llvm.org/t/clang-lld-thin-lto-footprint-and-run-time-performance-outperformed-by-gcc-ld/78997))

**Xtensa (ESP32) — the actual current state, not an assumption:**
- **GCC is the mature, default, officially-shipped toolchain** for Xtensa — `xtensa-esp32-elf-gcc`/unified `xtensa-esp-elf-gcc`, built via crosstool-NG, is what ESP-IDF ships and defaults to.
- **Espressif is genuinely investing in LLVM/Clang for Xtensa**, maintained at `espressif/llvm-project` (the older `espressif/llvm-xtensa` and `espressif/clang-xtensa` repos are now archived/merged into that unified fork). Active 2025 releases exist — e.g. tag `esp-19.1.2_20250225` (LLVM 19.1.2-based) — showing ongoing maintenance, not an abandoned experiment. ([github.com/espressif/llvm-project/releases](https://github.com/espressif/llvm-project/releases))
- There was a real upstream RFC to get a Tensilica Xtensa backend into **mainline** LLVM (`discourse.llvm.org` RFC thread, 2021), meaning the long-term direction is toward Xtensa being a first-class upstream LLVM target rather than permanently forked.
- **Current hybrid state**: Espressif's LLVM/Clang toolchain uses custom-prefixed `as`/`ld` binaries specifically to *avoid colliding* with the GCC-toolchain's binutils — meaning code generation goes through LLVM, but assembly/link currently still leans on binutils-derived tooling, with Espressif's stated plan being to replace these with an integrated assembler and LLD "in future." **This is not yet a clean-room, fully-LLVM pipeline.** ([esp32.com/viewtopic.php?t=9226](https://esp32.com/viewtopic.php?t=9226), [esp32.com/viewtopic.php?t=11412](https://esp32.com/viewtopic.php?t=11412))
- **ESP-IDF has had experimental Clang build support since IDF v5.0** via `IDF_TOOLCHAIN=clang` — explicitly experimental, with the stated intent to "gradually improve support for building IDF projects with clang" across all Xtensa targets (including S3) and RISC-V targets over time.
- **Recommendation**: treat GCC (`xtensa-esp-elf-gcc`) as the safe, production default for Xtensa size-critical firmware today; Clang/LLVM is usable and improving fast for tooling (clangd/LSP, static analysis) and worth revisiting for production codegen as Espressif's integrated-assembler/LLD work matures, but don't build Lw-Eh's size claims around Clang-on-Xtensa numbers yet.
- **RISC-V ESP32 variants are a different story**: RISC-V is natively supported in **mainline** LLVM (no fork needed), so Clang is a materially more mature, realistic option for esp32c3/c6/etc. than it is for Xtensa — worth noting as an asymmetry within the ESP32 family itself.

## B8. Newlib-nano and avoiding libstdc++ bloat

**C library choice:**
- **Classic embedded GCC pattern**: `--specs=nano.specs` swaps in `libc_nano.a`/`libm_nano.a` (ARM's stripped-down newlib fork — drops wide-char support, post-C89 additions, and slims `printf`/`scanf` family formatting); combine with `--specs=nosys.specs` to supply stub syscalls (`_sbrk`, `_write`, `_read`, etc.) for a true no-OS baremetal target: `gcc ... --specs=nano.specs --specs=nosys.specs`. Reported size reduction: roughly **30-70% smaller** than full newlib depending on what's used; one commonly cited example shows a "hello world" at roughly a third the flash and a tenth the SRAM versus plain newlib. ([metebalci.com](https://metebalci.com/blog/demystifying-arm-gnu-toolchain-specs-nano-and-nosys/), [mcuoneclipse.com](https://mcuoneclipse.com/2023/01/28/which-embedded-gcc-standard-library-newlib-newlib-nano/))
- **ESP32/ESP-IDF-specific and current as of this writing**: ESP-IDF has moved *past* newlib-nano as the size-optimized default. As of **ESP-IDF v6.0**, the project-wide default libc switched from Newlib to **PicolibC** (a newlib fork with a rewritten, more memory-efficient stdio implementation) — reported as smaller than even newlib-with-nano-formatting enabled (224,592 bytes vs. 239,888 bytes in Espressif's own comparison, ~6% smaller). Classic newlib (with or without `CONFIG_LIBC_NEWLIB_NANO_FORMAT`) is still selectable via `CONFIG_LIBC_NEWLIB` in menuconfig if needed for compatibility, but picolibc is now the size-optimized path Espressif itself recommends. ([developer.espressif.com — ESP-IDF 6.0 default libc](https://developer.espressif.com/blog/2026/04/esp-idf-6-default-libc-picolibc/), [docs.espressif.com migration guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.0/system.html)) **This is a meaningfully different answer than the "classic" newlib-nano advice** and should be surfaced prominently for any ESP32-facing guidance the project writes.
- AVR uses **avr-libc** — an entirely separate, already-minimal-by-design C library lineage, not newlib at all; the newlib-nano conversation doesn't apply there.

**Why a header-only C++ library must self-police to not defeat any of this** — Lw-Eh choosing a small libc for its *consumer* is necessary but not sufficient. Lw-Eh's own headers must not themselves demand heavy std-lib machinery that silently pulls in the expensive stuff regardless of libc choice:
- **Never `#include <iostream>`** transitively — even unused, `std::cout`/`std::cin` global objects force static-initialization machinery and drag in locale/streambuf infrastructure that's notoriously large. Same caution for `<sstream>`.
- **Avoid `<regex>`, `<thread>`, `<mutex>`, `<future>`** — threading-primitive headers assume a threading runtime is present/linked; regex is simply large.
- **Keep `-fno-exceptions`/`-fno-rtti` compatible**: Lw-Eh's API must not require `try`/`throw`/`dynamic_cast` to function — use error codes / `std::optional`-style returns instead, so consumers who globally disable exceptions/RTTI (as recommended in B4) can actually use the library.
- **Avoid `std::function`** for the dispatcher's callback storage — its type-erasure implementation typically involves heap allocation and can pull in `std::bad_function_call`/exception paths. Prefer a fixed-size function-pointer-plus-context-pointer pair, or a template-based static-dispatch callback list, sized at compile time (see Part A).
- **Avoid heap allocation** (`new`/`delete`) in the core dispatch path — `operator new`'s failure path is exception-shaped by default (`std::bad_alloc`) unless using `nothrow` overloads, and dynamic allocation is generally undesirable on MCU firmware regardless of size.
- **Avoid `std::vector`/`std::string`** dynamic containers for internal state; prefer fixed-capacity, compile-time-sized arrays/spans that the consumer provides storage for.
- **Be judicious with virtual functions** — each polymorphic class costs a vtable plus (unless `-fno-rtti`) a `type_info` object; for a "radically stripped-down EnTT dispatcher alternative," static/compile-time dispatch (templates, CRTP, or a fixed listener-table pattern) is more in keeping with the size goal than a virtual-interface-based observer pattern.
- **Watch template-instantiation bloat directly** — since every distinct event type instantiates fresh dispatcher code, consider funneling the bulk of the logic through a small number of non-templated (or minimally-templated) core functions that the thin templated public API calls into ("type erasure at the boundary"), so N event types don't produce N full copies of the entire dispatch machinery — this is the single highest-leverage code-size decision for a template-heavy header-only library, more impactful than any individual compiler flag.

## B9. Size-measurement / verification tooling

| Tool | Practical invocation | What it tells you |
|---|---|---|
| `size` (`<prefix>-size`, e.g. `arm-none-eabi-size`, `xtensa-esp32-elf-size`) | `arm-none-eabi-size -A firmware.elf` | Coarse `.text`/`.data`/`.bss` totals — the first, fastest sanity check; use `-A` (sysv format) for per-section breakdown instead of the default 3-bucket summary. |
| `nm --size-sort` | `arm-none-eabi-nm --size-sort --print-size -C firmware.elf \| tail -40` | Ranks symbols by size — the fastest way to find "what's the single biggest thing in here." `-C` demangles C++ names; `--radix=d` for decimal instead of hex. |
| `objdump` | `objdump -h firmware.elf` (section headers with sizes); `objdump -d -S firmware.elf` (disassembly interleaved with source); `objdump -t firmware.elf` (full symbol table) | Section-level and instruction-level inspection — use `-d -S` when you need to see *why* a specific function is larger than expected. |
| `readelf` | `readelf -S firmware.elf` (section headers); `readelf -sW firmware.elf \| sort -k3 -n` (symbols sorted by size, wide output); `readelf -x <section>` (raw hexdump of a section) | Similar niche to objdump, often preferred for scripting due to more stable/parseable output. |
| **Bloaty McBloatface** | `bloaty firmware.elf`; `bloaty -d symbols,compileunits firmware.elf`; diff mode: `bloaty new.elf -- old.elf` | The most practical tool here for attributing *every byte* to a symbol/compile-unit, with a default section/segment breakdown and a proper **diff mode** — exactly what's needed to verify "did this change move the needle on the 1-2KB target, and by how much, and where." Builds with CMake itself, all deps vendored as git submodules (usefully mirrors this project's own zero-network-fetch philosophy). ([github.com/google/bloaty](https://github.com/google/bloaty)) |
| `-Wl,-Map=out.map` | Add to link flags, then inspect `out.map` directly or grep it | Full linker-eye view: which object file/archive member pulled in which symbol, exact addresses/sizes of every placed section. Espressif's own docs note these can exceed 100,000 lines for a real firmware image — not meant for manual reading end-to-end, but invaluable for `grep`-ing a specific symbol's origin. ([docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/size.html)) |
| `-Wl,--print-gc-sections` | Add to link flags, inspect stderr/log output | Confirms `--gc-sections` is actually eliminating the sections you expect — critical verification step, since a missing `-ffunction-sections` on any one TU silently defeats the whole mechanism for that TU. |
| `idf.py size` / `size-components` / `size-files` (ESP-IDF-specific) | `idf.py size-components` | ESP-IDF's own wrapper (backed by the `esp-idf-size` Python tool) around the same map-file data, pre-aggregated per-component and per-source-file — the most convenient entry point specifically inside an ESP-IDF project, no manual map-file parsing needed. |

**Practical verification workflow for hitting a 1-2 KB target:**
1. Build a "control" firmware that links everything the target platform needs *without* Lw-Eh, record its `size`/Bloaty output.
2. Build the same firmware *with* Lw-Eh's headers included and exercised (not just included-but-unused, since an unused header contributes nothing — measure what actually gets instantiated/called), record again.
3. `bloaty with_lw_eh.elf -- without_lw_eh.elf` — the diff **is** Lw-Eh's true incremental footprint, cleanly separating it from the unavoidable platform-runtime floor described in B6.
4. If the diff exceeds budget, cross-reference `nm --size-sort` + `-Wl,-Map` to find which specific instantiated symbol is responsible, then check whether it's avoidable per the B8/Part A guidance (an accidental `std::function`, an unwanted vtable, a template instantiated more times than necessary, etc.).

## B10. Recommended CMakeLists.txt skeleton (illustrative sketch)

```cmake
cmake_minimum_required(VERSION 3.23)   # for target_sources(FILE_SET HEADERS)
project(LwEh LANGUAGES CXX)

add_library(lw_eh INTERFACE)
add_library(LwEh::lw_eh ALIAS lw_eh)

target_sources(lw_eh INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/lw_eh/dispatcher.hpp
          # ... remaining public headers
)
target_compile_features(lw_eh INTERFACE cxx_std_17)

# --- per-compiler condition helpers (generator expressions) ---
set(IS_GNU   "$<COMPILE_LANG_AND_ID:CXX,GNU>")
set(IS_CLANG "$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>")
set(IS_GCC_LIKE "$<OR:${IS_GNU},${IS_CLANG}>")

target_compile_options(lw_eh INTERFACE
    $<${IS_GNU}:-Os>
    $<${IS_CLANG}:-Oz>
    $<${IS_GCC_LIKE}:-ffunction-sections -fdata-sections>
    $<${IS_GCC_LIKE}:-fno-exceptions -fno-rtti>
    $<${IS_GCC_LIKE}:-fno-unwind-tables -fno-asynchronous-unwind-tables>
    $<${IS_GCC_LIKE}:-fno-threadsafe-statics -fno-use-cxa-atexit>
    $<${IS_GCC_LIKE}:-fomit-frame-pointer -fmerge-all-constants -fno-stack-protector>
    $<${IS_GCC_LIKE}:-fvisibility=hidden -fvisibility-inlines-hidden -fno-ident>
    $<${IS_GNU}:-flto -flto-partition=none>
    $<${IS_CLANG}:-flto=full>
)

target_link_options(lw_eh INTERFACE
    $<${IS_GCC_LIKE}:-Wl,--gc-sections>
    $<${IS_GNU}:-flto -flto-partition=none>
    $<${IS_CLANG}:-flto=full>
    $<${IS_GCC_LIKE}:-Wl,-Map=$<TARGET_NAME:lw_eh>.map>
)

# No find_package / FetchContent / ExternalProject_Add / vcpkg / Conan
# calls anywhere in this file, by design — see B1 hermeticity note.

install(TARGETS lw_eh EXPORT LwEhTargets FILE_SET HEADERS)
install(EXPORT LwEhTargets NAMESPACE LwEh:: DESTINATION lib/cmake/LwEh)
```

**Companion toolchain-file stub (ESP32 / Xtensa cross-compilation):**

```cmake
# toolchain-esp32-xtensa.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(CMAKE_C_COMPILER   xtensa-esp32-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp32-elf-g++)
set(CMAKE_ASM_COMPILER xtensa-esp32-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Seed initial flags via _INIT, never CMAKE_CXX_FLAGS directly (see B2)
set(CMAKE_CXX_FLAGS_INIT "-mlongcalls")
```

Usage: `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-esp32-xtensa.cmake` (swap compiler names/`-march=`/`-mabi=` for the RISC-V or ARM/AVR variants documented in B2). Note that a real ESP-IDF project would use ESP-IDF's own `idf.py`-managed toolchain files instead of a hand-rolled one (see B3) — this stub is for the "raw CMake + bare cross-compiler, no ESP-IDF" scenario.

---

## Research methodology note

This document was produced by two parallel research agents (general-purpose, no deep-research skill, no subagent spawning permitted), each performing its own direct web research and reporting back structured findings, which were then merged and cross-referenced into this single document. Claims are cited inline with source URLs where the agents pulled them from external sources; time-sensitive claims (toolchain defaults, standard-library versions, Clang/Xtensa maturity) are flagged as such and should be re-verified against whatever exact ESP-IDF/toolchain version the project ultimately pins, since these move fast.
