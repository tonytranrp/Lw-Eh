# ESP-IDF component wrapper -- resolves the dialect clash between plain CMake
# (the repo root CMakeLists.txt) and ESP-IDF's idf_component_register() macro
# without needing two competing root build files (Research/ARCHITECTURE.md).
# A consumer adds this directory's parent (i.e. the repo root) to
# EXTRA_COMPONENT_DIRS. Copying only this one directory into a consumer's own
# components/ tree does NOT work standalone: INCLUDE_DIRS below is "two
# directories up from this file," which only resolves to Lw-Eh's real
# include/ tree if the two stay at that exact relative depth -- vendor this
# directory together with the sibling include/ tree (e.g. a git submodule of
# the whole repo) if EXTRA_COMPONENT_DIRS isn't an option.
#
# ESP-IDF drives its own -Os/LTO via Kconfig (research.md §B3) -- the
# lw_eh_size_flags aggressive flag set from cmake/LwEhAggressiveFlags.cmake is
# deliberately NOT applied here.
#
# No REQUIRES: idf_component_register's own docs (Component CMakeLists Files,
# the zero-dependency `spark_plug` example) show REQUIRES simply omitted for
# a component with no dependencies, not passed as REQUIRES "" -- confirmed
# empirically harmless either way (cmake_parse_arguments treats REQUIRES ""
# and a bare/omitted REQUIRES identically, so this is an idiom fix, not a
# behavior change), but omitting it is what a real ESP-IDF component author
# would write, and says "zero deps, unconditionally, by design" rather than
# inviting the question of whether emptiness was meant to be conditional.

idf_component_register(
    INCLUDE_DIRS "../../include"
)
