# Bare freestanding toolchain file for Xtensa ESP32 (xtensa-esp32-elf-gcc).
# CI/example convenience only — Lw-Eh itself is INTERFACE-only and needs no
# toolchain file to be consumed. A real ESP-IDF project uses idf.py's own
# toolchain management instead (see integrations/esp-idf/), never this file.
# Requires xtensa-esp32-elf-gcc/-g++ already on PATH — never fetched here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(CMAKE_C_COMPILER   xtensa-esp32-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp32-elf-g++)
set(CMAKE_ASM_COMPILER xtensa-esp32-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Seeded via _INIT, never CMAKE_CXX_FLAGS directly, so a cache-supplied
# override isn't clobbered (research.md §B2 anti-pattern warning).
set(CMAKE_CXX_FLAGS_INIT "-mlongcalls")
set(CMAKE_C_FLAGS_INIT   "-mlongcalls")
