# Bare freestanding toolchain file for RISC-V ESP32 variants (esp32c3/c6/h2/
# c2/c5/c61), via riscv32-esp-elf-gcc. See xtensa-esp32.cmake for the general
# notes on why this file exists and what it doesn't cover.
# Requires riscv32-esp-elf-gcc/-g++ already on PATH — never fetched here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(CMAKE_C_COMPILER   riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-esp-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# rv32imc: the C (compressed instructions) extension is the RISC-V analog to
# ARM Thumb and is essential for code density (research.md §B4).
set(CMAKE_CXX_FLAGS_INIT "-march=rv32imc -mabi=ilp32")
set(CMAKE_C_FLAGS_INIT   "-march=rv32imc -mabi=ilp32")
