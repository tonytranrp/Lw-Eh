// Minimal bare-register RTC watchdog disable for real-hardware boot
// confirmation -- bare register pokes, no SDK/driver, matching this
// example's from-scratch ethos (examples/esp32_minimal/ has no ESP-IDF/
// Arduino runtime to call into). Not part of the Lw-Eh library itself --
// deliberately kept out of include/lweh/, this is example-only diagnostic
// scaffolding, same as uart.hpp.
//
// Register addresses verified directly against the real local
// Arduino-ESP32 framework headers (soc/rtc_cntl_reg.h, soc/soc.h):
// DR_REG_RTCCNTL_BASE = 0x3ff48000, RTC_CNTL_WDTWPROTECT_REG = base+0xa4
// (write-protect key register), RTC_CNTL_WDTCONFIG0_REG = base+0x8c
// (stage-0 config, bit 31 = RTC_CNTL_WDT_EN), RTC_CNTL_WDT_WKEY_VALUE =
// 0x50D83AA1 (the documented unlock key -- writing anything else to
// WDTWPROTECT_REG re-locks the watchdog config registers against writes).
//
// Why this is needed at all (Research/PROGRESS.md firing 46): the
// second-stage bootloader arms an RTC watchdog with a ~9s timeout
// (CONFIG_BOOTLOADER_WDT_TIME_MS=9000, per firing 40's ESP-IDF source
// investigation) as a safety net during its own early boot, and a real
// ESP-IDF/Arduino app disables or reconfigures it once the app takes
// over. This from-scratch app never did either, so real hardware
// confirmed (firing 46) the watchdog fires ~9s after app_main() starts
// running regardless of whether the app is doing anything wrong --
// even a correctly-running infinite loop (the firing-45 fix for the
// separate call0/windowed-ABI return crash) still gets reset once this
// timer expires unfed. Disabling it once, early in app_main(), before
// doing anything else, is simpler and sufficient for this example's
// needs than periodically feeding it.

namespace lweh_example {

inline void rtc_wdt_disable() {
    volatile unsigned* const wdt_wprotect = reinterpret_cast<volatile unsigned*>(0x3ff480a4);
    volatile unsigned* const wdt_config0 = reinterpret_cast<volatile unsigned*>(0x3ff4808c);
    *wdt_wprotect = 0x50D83AA1u; // unlock: RTC_CNTL_WDT_WKEY_VALUE
    *wdt_config0 = 0u;           // clear RTC_CNTL_WDT_EN (bit 31) and all stage config
    *wdt_wprotect = 0u;          // re-lock: anything other than the key value
}

} // namespace lweh_example
