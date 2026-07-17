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
    $<${LWEH_IS_GNU}:-Wl,--relax>
    $<${LWEH_IS_GNU}:-flto -flto-partition=none>
    $<${LWEH_IS_CLANG}:-flto=full>
)
# --relax is scoped to GNU only, not GCC_LIKE: research.md §B4 documents it
# as a GNU-binutils-ld-family flag (shrinks long call/load-address sequences
# once final addresses are known, notably on RISC-V and Xtensa). Confirmed
# empirically it must NOT be on the Clang path here -- this project's host
# Clang build links via llvm-mingw's bundled lld, which hard-errors with
# "unknown argument: --relax" rather than silently ignoring it. Compiler ID
# doesn't determine linker choice reliably enough to gate this on
# LWEH_IS_GCC_LIKE; GNU-vs-not is the safer proxy for "uses a binutils ld".

# Deliberately NOT adding a permanent -Wl,-Map=... here: lw_eh_size_flags is
# an INTERFACE library consumed by 8+ different targets (tests/, examples/*,
# size_audit/), and there's no simple, verified-safe generator expression
# for "this specific consuming target's name" to keep each one's map file
# from colliding with the others' -- research.md §B9's map-file workflow is
# still available on demand via a direct manual re-link (see
# Research/PROGRESS.md for prior examples), which is simpler and safer than
# risking a subtly wrong shared filename.
