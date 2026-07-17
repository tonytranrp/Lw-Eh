# Lw-Eh — Light-Weight Event Handler

A from-scratch, zero-dependency, header-only C++ event/signal-dispatch library for embedded microcontrollers (ESP32 and similar). Built as a radically stripped-down alternative to full ECS/event libraries like [EnTT](https://github.com/skypjack/entt) — same spirit of type-safe, allocation-free event dispatch, a fraction of the size.

**Status: actively under development.** The API below is implemented and tested, but not yet frozen — see [Research/PROGRESS.md](Research/PROGRESS.md) for exactly what's done and what's next.

## Why

Most C++ event/ECS libraries are general-purpose enough to handle almost any use case, which also makes them too large for size-constrained embedded targets. Lw-Eh picks a narrower scope on purpose:

- **No heap allocation, ever.** Listener storage is either a fixed-capacity array member or lives inside the listener object itself — never `new`/`malloc`.
- **No exceptions, no RTTI.** Builds clean under `-fno-exceptions -fno-rtti`.
- **No `std::function`, `std::vector`, `std::string`, `<iostream>`, or anything else that pulls in heap allocation or hundreds of KB of runtime support.**
- **Zero external dependencies.** Nothing is ever fetched by the build — no `FetchContent`, no vcpkg/Conan, no Boost.
- **Header-only.** No separate library to build or link against; `#include` it and go.

Measured incremental footprint on a real ESP32 (Xtensa) build: **471 bytes** for six event types split across both storage policies below — see [Research/PROGRESS.md](Research/PROGRESS.md) for the current number and how it's measured (this number moves as the reference scenario gains event types, so treat it as a snapshot, not a promise).

The reasoning behind every one of these choices — the exact compiler/linker flags, the delegate's binary layout, the reentrancy-safety analysis behind each storage policy, prior-art comparisons against EnTT/ETL/the Linux kernel — is written up in [Research/research.md](Research/research.md) and [Research/ARCHITECTURE.md](Research/ARCHITECTURE.md).

## Two storage policies

Lw-Eh gives you two ways to hold a set of listeners for an event type, and you pick per event type:

| | `lweh::signal<Event, N>` | `lweh::intrusive_signal<Event>` |
|---|---|---|
| Listener storage | Fixed-size array, capacity `N` (default 4) | Singly-linked list — the listener object *is* the node |
| Listener requirements | None — bind any free function or member function | Must inherit `lweh::intrusive_node<Event>` |
| Capacity | Bounded at compile time | Unbounded |
| Dispatch order | Attach order (FIFO) | Most-recently-attached first (LIFO) |
| Dispatcher footprint | `N × sizeof(delegate)` — 8 bytes/slot on a 32-bit target | One pointer |

Use `signal` when you know the listener count up front and don't want listeners to know anything about Lw-Eh. Use `intrusive_signal` when the listener count is unbounded, or the dispatcher itself needs to be cheap to embed in many places.

Both are safe to attach/detach — including a listener detaching itself — from *inside* a listener callback during dispatch, with the exact contract documented in each header; this was verified with adversarial, sanitizer-checked stress tests before either was written, not just asserted (see [Research/PROGRESS.md](Research/PROGRESS.md) for what that testing found, including a real bug it caught early).

## Usage

```cpp
#include <lweh/lweh.hpp>

struct button_event {
    unsigned pin;
    bool long_press;
};

void on_button(const button_event& e) {
    // ...
}

class logger {
public:
    void on_button(const button_event& e) { /* ... */ }
};

lweh::signal<button_event, 4> button_signal;
logger my_logger;

void setup() {
    button_signal.attach<&on_button>();                    // free function
    button_signal.attach<&logger::on_button>(&my_logger);  // member function
}

void loop() {
    button_signal.publish(button_event{2, true});
}
```

`intrusive_signal` looks like this instead:

```cpp
#include <lweh/intrusive_signal.hpp>

struct connection_event {
    bool connected;
};

class connection_logger : public lweh::intrusive_node<connection_event> {
public:
    void on_event(const connection_event& e) override { connected = e.connected; }
    bool connected = false;
};

lweh::intrusive_signal<connection_event> connection_signal;
connection_logger my_listener;

void setup() {
    connection_signal.attach(my_listener);
}
```

See [`examples/esp32_minimal/scenario.hpp`](examples/esp32_minimal/scenario.hpp) for a complete version of both, cross-compiled and measured on real ESP32 hardware.

## Integration

Lw-Eh is header-only — pick whichever of these fits your project:

- **Vendor directly**: copy (or git-submodule) the `include/lweh/` directory into your project and add it to your include path.
- **CMake `add_subdirectory`**: `add_subdirectory(path/to/Lw-Eh)`, then `target_link_libraries(your_target PRIVATE LwEh::lw_eh)`.
- **CMake `find_package`**: `cmake --install <build-dir> --prefix <prefix>` this repo, then `find_package(LwEh REQUIRED)` + `target_link_libraries(your_target PRIVATE LwEh::lw_eh)` against that prefix. [`packaging_check/`](packaging_check/) is a standalone, CI-verified fixture proving this path end-to-end — real install, real `find_package`, real dispatch through both storage policies.
- **ESP-IDF component**: add [`integrations/esp-idf/`](integrations/esp-idf/) to your project's `EXTRA_COMPONENT_DIRS`.
- **PlatformIO**: [`library.json`](library.json) is a standard PlatformIO library manifest.

Requires C++17. Nothing is ever downloaded by Lw-Eh's own build — see [Research/research.md](Research/research.md) (§B1) for how that's enforced.

## Building and testing this repo

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

This builds and runs the correctness test suite on your host machine — no embedded toolchain needed. `CMakePresets.json` also has presets for cross-compiling the examples to ESP32 (Xtensa and RISC-V), ARM Cortex-M, and AVR (see [`cmake/toolchains/`](cmake/toolchains/)), and a `size-host` preset that drives the size-measurement tooling in [`size_audit/`](size_audit/) described in [Research/research.md](Research/research.md) (§B9).

A `host-asan` preset (`cmake --preset host-asan && cmake --build --preset host-asan && ctest --preset host-asan`) runs the same suite compiled with `-fsanitize=address,undefined` — the reentrancy/dispatch-safety contracts documented in [`signal.hpp`](include/lweh/signal.hpp) and [`intrusive_signal.hpp`](include/lweh/intrusive_signal.hpp) were originally validated this way, and this preset keeps that coverage standing rather than one-off. Sanitizers are host-only by design: they're never applied to the flags examples/ cross-compile with, since bare-metal embedded targets have no sanitizer runtime available.

[`packaging_check/`](packaging_check/) separately verifies the `find_package(LwEh)` consumption path (see its own `CMakeLists.txt` for the exact commands) — a standalone project, deliberately not part of the main build, that installs the library to a scratch prefix and consumes it exactly as an external project would. Every CI run (`.github/workflows/ci.yml`) exercises `host-debug`, `host-asan`, `host-gcc` (real GNU GCC, not just Clang), the hermeticity lint, and this packaging check.

## License

MIT — see [LICENSE](LICENSE).
