// Minimal smoke example for AVR: avr-libc supplies crt0/startup, so a plain
// main() is the entry point here — unlike the other examples/*_minimal/,
// which are fully freestanding and need explicit startup stubs. NOT
// flashable production firmware. The actual usage logic lives in
// ../scenario.hpp, shared verbatim with the other three architectures'
// examples and size_audit/'s host-proxy probe so the example and the size
// number can never silently drift apart (Research/ARCHITECTURE.md: "one
// source of truth for realistic usage"). Brought up to date from its
// original Phase-0 stub state in firing 20 (Research/PROGRESS.md) -- this
// file used to be a dead stub that never called attach()/publish() at all,
// left behind after signal<> was actually implemented back in firing 3
// because no avr-gcc has ever been available on this machine to catch the
// staleness by trying to build it.
//
// Cross-reference against the four real, hardware-only bugs the ESP32/Xtensa
// example needed (see Research/PROGRESS.md and riscv32.ld's sibling note) —
// none transfer, for reasons specific enough to be worth recording rather
// than assumed, though unverified empirically since no avr-gcc is installed
// here to check:
//   - IRAM0's word-only-access restriction has no AVR equivalent. AVR is a
//     true Harvard split — flash and SRAM are genuinely separate address
//     spaces reached by different instructions (lpm/elpm vs ld/st/lds/sts),
//     not a permission bit inside one unified space — and SRAM itself has
//     no access-width restriction at all. Normal globals always live in
//     SRAM; the only way to put byte-accessed data in flash is the
//     deliberate, opt-in PROGMEM/pgm_read_* idiom, confirmed unused anywhere
//     in this repo.
//   - The AT>/LMA-vs-VMA copy-down pattern shouldn't recur either: avr-libc's
//     crt1 (__do_copy_data/__do_clear_bss, wired up via avr-gcc's own
//     per-MCU default linker script, selected through -mmcu=) is
//     decades-proven, unlike this project's hand-rolled from-scratch ESP32
//     linker script/startup.S which had no prior validation. The esptool.py
//     bug was that specific tool's bespoke elf2image never implementing
//     AT> at all; AVR's real flashing path goes through standard GNU
//     binutils objcopy, which honors LMA correctly.
//   - call0-into-windowed-ABI is categorically inapplicable: AVR has a flat
//     32x8-bit register file and no windowed-register concept whatsoever.
//   - The specific RTC-watchdog peripheral doesn't transfer, but the general
//     lesson does: AVR parts have their own WDT (WDTCSR/MCUSR), and
//     avr-libc's FAQ documents a real trap where the watchdog survives a
//     reset at a short timeout — if startup doesn't clear WDRF and
//     disable/reconfigure it early, the chip resets in a fast loop that
//     looks like a hang unrelated to Lw-Eh's own code.
// Not a new finding, just re-confirmed: ISR/concurrency safety is already
// explicitly out of scope project-wide (research.md §A9, signal.hpp,
// intrusive_core.hpp) — worth noting AVR's native 8-bit width makes that
// existing caveat bite more easily in practice, since pointers/size_t are
// 16-bit and any read-modify-write of one (e.g. intrusive_core's `next`
// link) is two non-atomic instructions without cli()/sei() or
// <util/atomic.h>'s ATOMIC_BLOCK.

#include "../scenario.hpp"

int main() {
    lweh_example::run_scenario(2, 42, true, 50, -60, true);
    for (;;) {
    }
}
