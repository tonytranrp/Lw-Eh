# Each example under examples/*_minimal/ is architecture-specific (its own
# linker script + entry-point convention, matching exactly one
# cmake/toolchains/*.cmake). Only add the one matching the currently active
# toolchain (CMAKE_SYSTEM_PROCESSOR, set by whichever toolchain file
# configured this build) -- adding all four unconditionally would mean a
# single-architecture configuration (e.g. Xtensa) tries to compile *and
# link* the other three using the wrong cross-compiler and the wrong
# linker script for their target.

# esp32_minimal/riscv32_esp_minimal's startup.S and arm_cortex_m_minimal's
# startup.c need their language enabled explicitly -- the root project()
# only declares CXX. Scoped here (not at the root) so host-only
# configurations (tests/, size_audit/) never pay for C/ASM compiler
# detection they don't need. Confirmed necessary the hard way: without this,
# CMake silently produced zero build steps for startup.S, _start went
# unresolved, and --gc-sections garbage-collected the entire example down to
# an empty 0-byte binary with no diagnostic beyond a linker warning.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "xtensa")
    enable_language(ASM)
    add_subdirectory(esp32_minimal)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv32")
    enable_language(ASM)
    add_subdirectory(riscv32_esp_minimal)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    enable_language(C)
    add_subdirectory(arm_cortex_m_minimal)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
    add_subdirectory(avr_minimal)
else()
    message(STATUS
        "examples: no matching embedded example for "
        "CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}' -- skipping. "
        "Configure with one of cmake/toolchains/*.cmake to build an example.")
endif()
