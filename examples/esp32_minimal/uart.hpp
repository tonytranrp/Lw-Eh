// Minimal polled-UART0 TX for real-hardware boot confirmation -- bare
// register pokes, no SDK/driver, matching this example's from-scratch
// ethos (examples/esp32_minimal/ has no ESP-IDF/Arduino runtime to call
// into). Not part of the Lw-Eh library itself -- deliberately kept out of
// include/lweh/, this is example-only diagnostic scaffolding.
//
// Register map and POR-state claims verified directly against the real
// local Arduino-ESP32 framework headers (soc/uart_reg.h, soc/dport_reg.h,
// soc/io_mux_reg.h) and cross-checked via nm against the real prebuilt
// bootloader ELF (Research/PROGRESS.md firing 39): on classic ESP32,
// UART0 is clocked and out of reset from power-on (DPORT_PERI_CLK_EN_REG
// POR default has UART_CLK_EN and UART_MEM_CLK_EN both set), its TX/RX
// pins are routed to GPIO1/3 by default pad function (not GPIO-matrix),
// and its POR baud divisor (694) already yields ~115200 8N1 -- matching
// this project's own already-captured 115200 boot-log. No enable/routing
// step is required on the boot path this project's startup.S runs
// (strictly after the ROM and the 2nd-stage bootloader, both of which
// already rely on UART0 working this way). The baud rewrite below is
// defensive insurance, not a requirement.
//
// Used identically from both examples/esp32_minimal/main.cpp (the
// with-lweh measurement side) and size_audit/control_esp32_main.cpp (the
// control side) so size_audit's incremental-cost diff isolates only
// Lw-Eh's own contribution, not this logging's cost on one side only.

namespace lweh_example {

inline void uart0_init_baud_defensive() {
    volatile unsigned* const clkdiv = reinterpret_cast<volatile unsigned*>(0x3ff40014);
    *clkdiv = 694; // 80MHz / 694 ~= 115273.8 baud, ~0.06% off nominal 115200
}

inline void uart0_tx_byte(char c) {
    volatile unsigned* const status = reinterpret_cast<volatile unsigned*>(0x3ff4001c);
    volatile unsigned* const fifo = reinterpret_cast<volatile unsigned*>(0x3ff40000);
    while (((*status >> 16) & 0xFFu) >= 128u) {
        // wait for TX FIFO room (UART_TXFIFO_CNT, status bits [23:16])
    }
    *fifo = static_cast<unsigned>(static_cast<unsigned char>(c));
}

inline void uart0_tx_string(const char* s) {
    while (*s) {
        uart0_tx_byte(*s++);
    }
}

} // namespace lweh_example
