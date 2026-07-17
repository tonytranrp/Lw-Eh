# Cross-compiled freestanding smoke example — proves the headers compile and
# link cleanly for the Xtensa ESP32 ABI under the aggressive flag set, and
# doubles as size_audit/'s "with_lweh" payload (Research/ARCHITECTURE.md).
# Real bootable startup/memory-map work landed in firing 38 (see esp32.ld /
# startup.S) validated against a real connected board.
# Build with: cmake --preset esp32-xtensa (requires xtensa-esp32-elf-gcc on PATH).

add_executable(lw_eh_example_esp32_minimal main.cpp startup.S)
target_link_libraries(lw_eh_example_esp32_minimal PRIVATE LwEh::lw_eh lw_eh_size_flags)
target_link_options(lw_eh_example_esp32_minimal PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/esp32.ld -nostdlib
)

# NOTE: a call0-ABI fix was attempted here (to remove entry/retw windowed
# prologues from this binary) and does NOT work with this specific
# prebuilt toolchain -- confirmed via `-print-multi-lib`, this esp-2021r2
# crosstool-NG build has no call0 multilib variant at all (only
# ".;", "esp32-psram;...", "no-rtti;...", "esp32-psram/no-rtti;..."), and
# xtensa-esp32-elf-g++ rejects -mabi=call0/--abi-call0 outright as
# unrecognized. Whether windowed entry/retw (confirmed via objdump: 20
# occurrences in this binary) actually runs correctly when entered via
# esptool load_ram -- i.e. whether the boot ROM's UART download-mode
# monitor already leaves PS.WOE + window exception vectors configured --
# is an open question being resolved empirically on real hardware instead
# (see Research/PROGRESS.md firing 38). Do not re-attempt -mabi=call0 on
# this toolchain; if windowed ABI turns out not to work from load_ram, the
# real fix is installing minimal window exception vectors in startup.S,
# not a compiler flag.
