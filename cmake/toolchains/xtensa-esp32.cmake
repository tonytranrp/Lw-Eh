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
#
# -ffreestanding (implies -fno-builtin): research.md §B4 frames -fno-builtin
# as generally NOT a blanket win and recommends leaving it off, "unless
# writing a from-scratch runtime with non-standard libc semantics" -- which
# is exactly this target's situation (examples/esp32_minimal/ links
# -nostdlib, has no libc at all, and its own from-scratch startup.S/esp32.ld
# are the only "runtime" that exists). Without it, the compiler is free to
# assume hosted memcpy/memset/etc. semantics and could in principle emit a
# call to a symbol that plain -nostdlib linking will never provide -- a
# latent risk rather than an observed failure so far (the real-hardware
# validation this project already completed never happened to trigger it,
# since none of this code currently does anything a builtin substitution
# would fire for), closed here rather than left for a future firing to
# discover the hard way once the code does.
set(CMAKE_CXX_FLAGS_INIT "-mlongcalls -ffreestanding")
set(CMAKE_C_FLAGS_INIT   "-mlongcalls -ffreestanding")
