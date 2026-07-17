# Mirrors examples/esp32_minimal/ for the RISC-V ESP32 family (esp32c3/c6/...).
# See esp32_minimal/CMakeLists.txt for the full rationale — not repeated here.
# Build with: cmake --preset esp32-riscv (requires riscv32-esp-elf-gcc on PATH).

add_executable(lw_eh_example_riscv32_esp_minimal main.cpp startup.S)
target_link_libraries(lw_eh_example_riscv32_esp_minimal PRIVATE LwEh::lw_eh lw_eh_size_flags)
target_link_options(lw_eh_example_riscv32_esp_minimal PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/riscv32.ld -nostdlib
)
