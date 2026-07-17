# Aggressive size/perf flag set (research.md Part B §B4), isolated in its own
# INTERFACE target rather than attached to lw_eh itself. Consumed only by
# tests/, examples/*, and size_audit/ — never propagated to consumers of
# lw_eh, which would otherwise silently change how a consumer's own unrelated
# code compiles and would break ESP-IDF component consumption (research.md
# §B3). See Research/ARCHITECTURE.md "Reconciliation 2" for the full reasoning.

add_library(lw_eh_size_flags INTERFACE)

set(LWEH_IS_GNU   "$<COMPILE_LANG_AND_ID:CXX,GNU>")
set(LWEH_IS_CLANG "$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>")
set(LWEH_IS_GCC_LIKE "$<OR:${LWEH_IS_GNU},${LWEH_IS_CLANG}>")

target_compile_options(lw_eh_size_flags INTERFACE
    $<${LWEH_IS_GNU}:-Os>
    $<${LWEH_IS_CLANG}:-Oz>
    $<${LWEH_IS_GCC_LIKE}:-ffunction-sections -fdata-sections>
    $<${LWEH_IS_GCC_LIKE}:-fno-exceptions -fno-rtti>
    $<${LWEH_IS_GCC_LIKE}:-fno-unwind-tables -fno-asynchronous-unwind-tables>
    $<${LWEH_IS_GCC_LIKE}:-fno-threadsafe-statics -fno-use-cxa-atexit>
    $<${LWEH_IS_GCC_LIKE}:-fomit-frame-pointer -fmerge-all-constants -fno-stack-protector>
    $<${LWEH_IS_GCC_LIKE}:-fvisibility=hidden -fvisibility-inlines-hidden -fno-ident>
    $<${LWEH_IS_GNU}:-flto -flto-partition=none>
    $<${LWEH_IS_CLANG}:-flto=full>
)

target_link_options(lw_eh_size_flags INTERFACE
    $<${LWEH_IS_GCC_LIKE}:-Wl,--gc-sections>
    $<${LWEH_IS_GNU}:-flto -flto-partition=none>
    $<${LWEH_IS_CLANG}:-flto=full>
)
