# Mirrors examples/esp32_minimal/ for AVR. No custom linker script needed —
# avr-gcc's -mmcu= (set via LWEH_AVR_MCU in cmake/toolchains/avr.cmake)
# supplies a correct default memory map, and avr-libc supplies crt0/startup.
# Build with: cmake --preset avr (requires avr-gcc on PATH).

add_executable(lw_eh_example_avr_minimal main.cpp)
target_link_libraries(lw_eh_example_avr_minimal PRIVATE LwEh::lw_eh lw_eh_size_flags)
