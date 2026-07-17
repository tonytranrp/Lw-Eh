# Size-audit machinery — structurally separate from tests/ (never a CTest
# case; research.md §B9, Research/ARCHITECTURE.md). Builds a "control" binary
# that never includes any lweh header, and reuses examples/esp32_minimal
# verbatim as the "with_lweh" binary, so size_report.py's Bloaty diff isolates
# Lw-Eh's true incremental footprint from the platform runtime floor
# (research.md §B6).

add_executable(lw_eh_size_audit_control control_main.cpp)
target_link_libraries(lw_eh_size_audit_control PRIVATE lw_eh_size_flags)

if(TARGET lw_eh_example_esp32_minimal)
    # Reuses the real example target directly rather than recompiling — one
    # source of truth for "realistic Lw-Eh usage" (Research/ARCHITECTURE.md).
    add_custom_target(lw_eh_size_audit_with_lweh DEPENDS lw_eh_example_esp32_minimal)
else()
    message(STATUS
        "size_audit: lw_eh_example_esp32_minimal not built (enable LWEH_BUILD_EXAMPLES "
        "and configure with cmake/toolchains/xtensa-esp32.cmake) — with-lweh size target skipped.")
endif()
