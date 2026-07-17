# Lw-Eh Architecture Decision (Phase 0)

**Status:** Skeleton scaffolded, no real implementation yet. This is the synthesized decision from two independent subagent proposals (library-header architecture and CMake/test/tooling architecture), reconciled by the architect of record into one final tree. See [research.md](research.md) for the underlying technical research this all traces back to.

**Post-Phase-0 amendment (core-implementation phase):** the "`signal` privately inherits `detail::signal_core`" design below is implemented differently than originally planned, and it's worth being explicit about why. Making `signal_core` genuinely event-erased (able to hold/dispatch listeners for *any* `Event` type without being templated on it) requires an extra adapter-stub layer on top of `delegate<Sig>`'s own stub — `delegate<void(const Event&)>` can't be stored in a non-template array without erasing `Event` itself, which means every `attach()` call would need to generate a second, event-erasing trampoline in addition to `delegate`'s own. That's real added complexity for a size win research.md §A4 itself says only pays off "past small single digits" of event types — and this codebase doesn't have that many yet to justify it against, and §A10 is explicit that size claims should be measured, not assumed. So `signal<Event, MaxListeners>` (`include/lweh/signal.hpp`) is implemented as a **self-contained, fully-templated type for now** — no inheritance from `detail::signal_core`, which stays a documented-as-deferred stub. This is a scoping decision made by the architect of record under the discretion Phase 0 established (informed by research.md's own guidance, not a new value judgment), not a silent abandonment of §A4 — revisit once there's real multi-event-type size data showing the naive per-type cost is actually a problem.

## How this document was produced

Two subagents each proposed half of the architecture independently, without seeing each other's output:
- **Subagent A** proposed the public header/folder layout, namespace, API surface, and naming conventions (traces to research.md Part A).
- **Subagent B** proposed the CMake project layout, test/tooling tree, toolchain files, and size-audit machinery (traces to research.md Part B).

Both were thorough and are adopted almost entirely as proposed. Three reconciliations were needed where they didn't quite line up, plus two departures from what B suggested (B's mandate was CMake/test structure, so a couple of header-level suggestions it made in passing are overruled by A's more authoritative treatment of that exact question). Each is recorded below with reasoning.

## Reconciliation 1 — namespace/include-dir vs. CMake-target naming domains

Subagent A used `lweh` consistently for the C++ namespace *and* the `include/lweh/` directory (this directly follows research.md §A5's explicit instruction to wrap everything in `namespace lweh {}`). Subagent B used `lw_eh` for the CMake target (`add_library(lw_eh INTERFACE)`, alias `LwEh::lw_eh`), matching research.md's own §B10 illustrative skeleton.

**Decision: keep both, in their own domains, deliberately.**
- C++ identifiers (namespace, include directory, header filenames): **`lweh`** — no underscore, matches how it's actually typed at every call site (`lweh::signal<...>`, `#include <lweh/signal.hpp>`).
- CMake identifiers (target name, alias, options, cache variables): **`lw_eh`** / **`LWEH_`** — matches the repo name `Lw-Eh`, matches research.md's own existing CMake sketch, and matches ordinary CMake convention (e.g. the `nlohmann_json` CMake target backing `nlohmann::json` in C++ — Subagent A raised this exact precedent independently).

This isn't a compromise so much as recognizing these are two different naming domains that routinely diverge in real C++ projects. Concretely: CMake target `lw_eh`, alias `LwEh::lw_eh`, options `LWEH_BUILD_TESTS`/`LWEH_BUILD_EXAMPLES`/`LWEH_BUILD_SIZE_AUDIT`, helper target `lw_eh_size_flags`; C++ namespace `lweh`, directory `include/lweh/`.

## Reconciliation 2 — the aggressive compiler/linker flag set is NOT attached to the library target itself

research.md §B10's illustrative CMakeLists.txt sketch attached the full aggressive flag set (`-Os`/`-Oz`, `-fno-exceptions`, `-fno-rtti`, `-flto`, etc.) directly to `lw_eh`'s `INTERFACE_COMPILE_OPTIONS`. Subagent B explicitly flagged and departed from this in its own proposal, and that departure is correct — adopted here:

- Forcing those flags onto every consumer via `target_link_libraries(app PRIVATE LwEh::lw_eh)` would silently change how the *consumer's own unrelated code* in that translation unit compiles (exceptions/RTTI toggled, LTO forced on) — not a header-only library's place to dictate.
- It directly contradicts research.md §B3's own instruction that the aggressive set must never be force-applied inside an ESP-IDF component context (ESP-IDF drives its own `-Os`/LTO via Kconfig).
- `-Wl,-Map=lw_eh.map` as a *propagated* link option is actively broken — every consumer would collide on the same map-file name.

**Decision:** the flag set lives in a standalone `cmake/LwEhAggressiveFlags.cmake` helper `INTERFACE` target (`lw_eh_size_flags`), consumed only by `tests/`, `examples/*/`, and `size_audit/` — never linked into `lw_eh` itself. `lw_eh` itself carries only `target_compile_features(lw_eh INTERFACE cxx_std_17)` and its include-directory propagation. This is the one place this document overrules research.md's own illustrative sketch; the reasoning above is why.

## Reconciliation 3 — no `dispatcher.hpp` / runtime type registry in v1

Subagent B's directory-tree answer, written for a CMake/test question rather than a header-design one, incidentally sketched a `dispatcher.hpp` + `detail/dispatcher_core.hpp` pair described as a "multi-event-type dispatcher." **This is rejected for v1.** research.md §A2 is explicit and deliberate on this point: EnTT's `dispatcher` — a runtime type-keyed registry (`dense_map<id_type, handler>`) letting arbitrary code attach to arbitrary event types by runtime lookup — is exactly the kind of unrequested generality Lw-Eh exists to not have. Subagent A's proposal (each event type gets its own compile-time-bound `signal<Event, N>`, no runtime type id, no lookup) is the settled design and is what's scaffolded. If a runtime-dispatch layer is ever justified by a real use case, it's a deliberate, separately-evaluated addition later — not default v1 surface area.

## Final decisions adopted as proposed

- **Multi-header, not single-header**, source-of-truth (Subagent A §1) — all three consumption paths (git-submodule vendoring, ESP-IDF component, PlatformIO library) take a directory, not a file; compile time is explicitly not a constraint; the §A4 core/shim split deserves to be a filesystem boundary during active pre-1.0 iteration. A generated single-file amalgamation for hobbyist Arduino-IDE-style drop-in may be offered later as a *release artifact*, never as the edited source.
- **`lweh` / `lweh::detail` namespace split** (Subagent A §2) — public API flat in `lweh`; the non-template engines (`signal_core`, `intrusive_core`) in `lweh::detail`, signaling "not the contract, don't reach in here."
- **Vocabulary: `bind()` (delegate-level), `attach()`/`detach()` (signal-level), `publish()` (fire an event), `on_event()` (intrusive listener hook)** (Subagent A §5) — `dispatch()` reserved for the internal engine layer, deliberately not exposed publicly, keeping "user calls `publish()`, which calls into `dispatch()`" as a clean two-layer vocabulary. `attach`/`detach` chosen over `connect`/`disconnect` specifically to avoid colliding with networking/peripheral "connect" terminology in the same embedded codebases this ships into, and to match Arduino's own `attachInterrupt()`/`detachInterrupt()` precedent.
- **Storage: static NTTP-sized array is the unconditional default for `signal`; `intrusive_signal`/`intrusive_node` is a separate, explicitly-opted-into type**, not a policy template parameter on `signal` (Subagent A §3) — the two storage strategies have genuinely different call-site contracts (array-backed requires nothing from the listener; intrusive requires inheriting a base and overriding a hook), so folding both into one templated `signal<Event, N, Policy>` would add an axis of generality most users never touch. A consumer who never includes `intrusive_signal.hpp` compiles zero bytes of it.
- **Hand-rolled test harness, not Catch2/GoogleTest/doctest** (Subagent B §2) — CTest already supplies the runner/parallelism/aggregation; a framework would only contribute assertion macros, which a ~80-line header does just as well without a few hundred KB of vendored source sitting in a project whose whole identity is minimalism, and without GoogleTest's non-header-only build-a-static-lib-first requirement.
- **`CMakePresets.json`** (Subagent B §6) — `host-debug` as the default preset (no toolchain file, tests on), one preset per embedded toolchain family (tests off, examples on), size-audit presets layered on top. CI and local contributors use the same named presets, so there's one build recipe, not two that can drift.
- **`size_audit/` is structurally decoupled from `tests/`** (Subagent B §4) — never registered as a CTest case; a separate `size_report.py` orchestrates `size`/`nm --size-sort`/Bloaty-diff-mode/`--print-gc-sections` verification, diffing a `control` build (no Lw-Eh) against a `with_lweh` build that reuses `examples/esp32_minimal/main.cpp` verbatim, so the "realistic usage" sample used for size measurement can never silently drift from the documented example.
- **`integrations/esp-idf/`** as a separate wrapper directory with its own `CMakeLists.txt` doing `idf_component_register(INCLUDE_DIRS ../../include REQUIRES "")` (Subagent B §7) — resolves the dialect clash between plain CMake and ESP-IDF's component-registration macros without needing two competing root build files.

## Final directory tree (Phase 0 — stubs only, no real logic yet)

```
CMakeLists.txt                          # root: lw_eh INTERFACE target, LWEH_BUILD_* options, install/export
CMakePresets.json                       # host-debug (default) + per-toolchain presets
library.json                            # PlatformIO manifest, includeDir -> include/

Research/
  research.md                           # pre-existing
  ARCHITECTURE.md                       # this file
  PROGRESS.md                           # durable ledger, updated every firing

cmake/
  toolchains/
    xtensa-esp32.cmake                  # bare xtensa-esp32-elf-gcc freestanding toolchain
    riscv32-esp.cmake                   # bare riscv32-esp-elf-gcc freestanding toolchain
    arm-cortex-m.cmake                  # bare arm-none-eabi-gcc freestanding toolchain (LWEH_CORTEX_M_CPU)
    avr.cmake                           # bare avr-gcc freestanding toolchain (LWEH_AVR_MCU)
  LwEhAggressiveFlags.cmake             # INTERFACE helper target lw_eh_size_flags carrying the §B4 flags
  LwEhConfig.cmake.in                   # template backing downstream find_package(LwEh)

include/lweh/
  lweh.hpp                              # umbrella header
  config.hpp                            # default_max_listeners, version macros, feature-test shims
  delegate.hpp                          # delegate<Sig> — 2-pointer non-owning callback (research.md §A1)
  signal.hpp                            # signal<Event, MaxListeners> — thin shim over detail::signal_core
  intrusive_signal.hpp                  # intrusive_node<Event> + intrusive_signal<Event> — alternate storage
  detail/
    signal_core.hpp                     # shared non-template dispatch engine (research.md §A4)
    intrusive_core.hpp                  # shared non-template intrusive-list engine

integrations/esp-idf/
  CMakeLists.txt                        # idf_component_register wrapper, zero deps
  idf_component.yml                     # optional ESP Component Registry manifest

tests/
  CMakeLists.txt                        # explicit test-case list -> add_executable + add_test
  test_harness.hpp                      # hand-rolled assert-style harness, zero deps
  attach_detach_test.cpp
  dispatch_order_test.cpp
  capacity_boundary_test.cpp            # covers both N and N+1 listeners
  reentrancy_test.cpp                   # self-detach-during-dispatch
  multi_event_isolation_test.cpp

examples/
  esp32_minimal/{CMakeLists.txt,main.cpp,esp32.ld,startup.S}
  riscv32_esp_minimal/{CMakeLists.txt,main.cpp,riscv32.ld,startup.S}
  arm_cortex_m_minimal/{CMakeLists.txt,main.cpp,cortex_m.ld,startup.c}
  avr_minimal/{CMakeLists.txt,main.cpp}   # no linker script needed, -mmcu= supplies the default

size_audit/
  CMakeLists.txt                        # size_audit_control + size_audit_with_lweh targets
  control_main.cpp                      # baseline skeleton, never includes lweh
  size_report.py                        # size/nm/bloaty-diff/--print-gc-sections orchestration
```

## Environment note (recorded here so later firings don't re-discover it)

This dev machine has CMake 4.3.2 and Clang 22.1.4 (MinGW target — `g++` on PATH also resolves to this same Clang, not real GCC). **No embedded cross-compilers are installed** (`xtensa-esp32-elf-gcc`, `riscv32-esp-elf-gcc`, `arm-none-eabi-gcc`, `avr-gcc` are all absent). This means:
- Host builds/tests (the `host-debug` preset, `tests/`) are fully achievable and should be the default verification for every firing.
- The four `examples/*` and the ESP32 branch of `size_audit/` cannot be cross-compiled or size-measured *on this machine* until a toolchain is installed — installing one wasn't done autonomously since it's a nontrivial environment change (large downloads, PATH modification). The `size-host` (Clang-only) size-audit path is available now and stands in as the interim proxy signal per research.md §B9's own "host-representative" flavor.
- This is a known, tracked limitation, not a silently-skipped step — real embedded validation should happen the moment a toolchain becomes available (e.g. if the user installs one, or a firing runs in an environment that has one).
