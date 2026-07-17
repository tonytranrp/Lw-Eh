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
else()
    message(STATUS
        "size_audit: real embedded with-lweh target skipped (enable "
        "LWEH_BUILD_EXAMPLES and configure with a toolchain from "
        "cmake/toolchains/) — lw_eh_size_audit_with_lweh_host remains "
        "available as the host-representative proxy signal.")
endif()
