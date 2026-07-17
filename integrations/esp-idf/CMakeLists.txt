# ESP-IDF component wrapper -- resolves the dialect clash between plain CMake
# (the repo root CMakeLists.txt) and ESP-IDF's idf_component_register() macro
# without needing two competing root build files (Research/ARCHITECTURE.md).
# A consumer adds this directory's parent to EXTRA_COMPONENT_DIRS, or copies
# this directory into their own components/ tree.
#
# ESP-IDF drives its own -Os/LTO via Kconfig (research.md §B3) -- the
# lw_eh_size_flags aggressive flag set from cmake/LwEhAggressiveFlags.cmake is
# deliberately NOT applied here.

idf_component_register(
    INCLUDE_DIRS "../../include"
    REQUIRES ""
)
