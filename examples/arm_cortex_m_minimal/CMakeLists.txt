# Mirrors examples/esp32_minimal/ for ARM Cortex-M.
# See esp32_minimal/CMakeLists.txt for the full rationale — not repeated here.
# Build with: cmake --preset cortex-m (requires arm-none-eabi-gcc on PATH).

add_executable(lw_eh_example_arm_cortex_m_minimal main.cpp startup.c)
target_link_libraries(lw_eh_example_arm_cortex_m_minimal PRIVATE LwEh::lw_eh lw_eh_size_flags)
target_link_options(lw_eh_example_arm_cortex_m_minimal PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/cortex_m.ld -nostdlib
)
