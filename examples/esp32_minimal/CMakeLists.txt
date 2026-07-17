# Cross-compiled freestanding smoke example — proves the headers compile and
# link cleanly for the Xtensa ESP32 ABI under the aggressive flag set, and
# doubles as size_audit/'s "with_lweh" payload (Research/ARCHITECTURE.md).
# NOT flashable production firmware — no real startup/memory-map work has
# been done yet (see the TODOs in startup.S / esp32.ld).
# Build with: cmake --preset esp32-xtensa (requires xtensa-esp32-elf-gcc on PATH).

add_executable(lw_eh_example_esp32_minimal main.cpp startup.S)
target_link_libraries(lw_eh_example_esp32_minimal PRIVATE LwEh::lw_eh lw_eh_size_flags)
target_link_options(lw_eh_example_esp32_minimal PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/esp32.ld -nostdlib
)
