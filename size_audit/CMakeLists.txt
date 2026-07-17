# Size-audit machinery — structurally separate from tests/ (never a CTest
# case; research.md §B9, Research/ARCHITECTURE.md). Two "with_lweh" targets
# against the same "control" baseline (which never includes any lweh
# header): a host-representative one (always buildable, no cross-toolchain
# needed -- research.md §B9's "host-representative... fast, always-available
# proxy signal" flavor) and a real embedded one (only when cross-compiled
# via LWEH_BUILD_EXAMPLES + a matching toolchain file from cmake/toolchains/).

add_executable(lw_eh_size_audit_control control_main.cpp)
target_link_libraries(lw_eh_size_audit_control PRIVATE lw_eh_size_flags)

add_executable(lw_eh_size_audit_with_lweh_host with_lweh_main.cpp)
target_link_libraries(lw_eh_size_audit_with_lweh_host PRIVATE LwEh::lw_eh lw_eh_size_flags)

if(TARGET lw_eh_example_esp32_minimal)
    # Reuses the real embedded example target directly rather than
    # recompiling — one source of truth for "realistic Lw-Eh usage"
    # (Research/ARCHITECTURE.md), for when a real cross-toolchain is
    # available (see Research/PROGRESS.md environment note).
    add_custom_target(lw_eh_size_audit_with_lweh_embedded DEPENDS lw_eh_example_esp32_minimal)

    # Matching ESP32 control baseline: same startup.S/esp32.ld boot path as
    # lw_eh_example_esp32_minimal, so the two are "otherwise identical
    # builds" per research.md B9 -- see control_esp32_main.cpp. Pass
    # --control-target lw_eh_size_audit_control_esp32 to size_report.py
    # when measuring against lw_eh_example_esp32_minimal (the default
    # lw_eh_size_audit_control has no custom boot path and understates
    # Lw-Eh's true incremental cost once the with-lweh side has one).
    #
    # enable_language(ASM) again here: examples/CMakeLists.txt already
    # called it, but only within its own directory scope (a sibling of
    # size_audit/ under the root CMakeLists.txt, not an ancestor) --
    # confirmed the hard way, a fresh esp32-xtensa configure with this
    # block missing failed with "required internal CMake variable not set:
    # CMAKE_ASM_COMPILE_OBJECT". enable_language() is idempotent, so this
    # is a no-op everywhere examples/ already enabled it and only actually
    # does something in size_audit/'s own separate directory scope.
    enable_language(ASM)
    add_executable(lw_eh_size_audit_control_esp32
        control_esp32_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/esp32_minimal/startup.S
    )
    target_link_libraries(lw_eh_size_audit_control_esp32 PRIVATE lw_eh_size_flags)
    target_link_options(lw_eh_size_audit_control_esp32 PRIVATE
        -T${CMAKE_CURRENT_SOURCE_DIR}/../examples/esp32_minimal/esp32.ld -nostdlib
    )
else()
    message(STATUS
        "size_audit: real embedded with-lweh target skipped (enable "
        "LWEH_BUILD_EXAMPLES and configure with a toolchain from "
        "cmake/toolchains/) — lw_eh_size_audit_with_lweh_host remains "
        "available as the host-representative proxy signal.")
endif()
