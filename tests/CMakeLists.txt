# Host CTest suite — sole purpose is FUNCTIONAL/BEHAVIORAL CORRECTNESS of the
# library's logic. Structurally separate from size_audit/ (research.md §B9,
# Research/ARCHITECTURE.md): this suite never measures size, size_audit/ is
# never registered as a CTest case.
#
# One executable + one add_test() per correctness concern (Research/PROGRESS.md
# fills these in during Phase 1 as the corresponding library feature lands).

set(LWEH_TEST_CASES
    delegate
    attach_detach
    dispatch_order
    capacity_boundary
    reentrancy
    multi_event_isolation
    intrusive_signal
)

# LWEH_SANITIZE (host-only, opt-in -- see root CMakeLists.txt) applies
# -fsanitize directly to each test target, NOT to lw_eh_size_flags itself:
# that INTERFACE target is also linked by examples/*'s cross-compiled
# embedded binaries, which have no sanitizer runtime available for their
# bare-metal targets. Every reentrancy/dispatch-safety contract this suite
# checks was originally validated by one-off ASan/UBSan-compiled stress
# tests during design (Research/PROGRESS.md, firings 8-9) -- this wires the
# same coverage permanently into the standing suite instead of leaving it as
# a one-time investigation artifact, so a future regression can't silently
# pass all 7 tests just because none of them happens to crash outright.
if(LWEH_SANITIZE)
    set(LWEH_SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

foreach(case IN LISTS LWEH_TEST_CASES)
    add_executable(lw_eh_test_${case} ${case}_test.cpp)
    target_link_libraries(lw_eh_test_${case} PRIVATE LwEh::lw_eh lw_eh_size_flags)
    if(LWEH_SANITIZE)
        target_compile_options(lw_eh_test_${case} PRIVATE ${LWEH_SANITIZE_FLAGS})
        target_link_options(lw_eh_test_${case} PRIVATE ${LWEH_SANITIZE_FLAGS})
    endif()
    add_test(NAME ${case} COMMAND lw_eh_test_${case})
endforeach()
