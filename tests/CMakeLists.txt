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
)

foreach(case IN LISTS LWEH_TEST_CASES)
    add_executable(lw_eh_test_${case} ${case}_test.cpp)
    target_link_libraries(lw_eh_test_${case} PRIVATE LwEh::lw_eh lw_eh_size_flags)
    add_test(NAME ${case} COMMAND lw_eh_test_${case})
endforeach()
