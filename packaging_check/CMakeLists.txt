# Standalone project deliberately NOT add_subdirectory'd by the root
# CMakeLists.txt -- it exists to prove the find_package(LwEh) consumption
# path research.md §B1 documents (install(EXPORT ...)/configure_package_
# config_file()/write_basic_package_version_file() in the root
# CMakeLists.txt) actually works end-to-end against a real installed tree,
# which structurally can't be tested from inside the same configure run that
# produces the package in the first place. Every other consumption path in
# this repo (tests/, examples/, size_audit/) uses add_subdirectory, which
# never exercises install()/find_package() at all -- until firing 31, this
# whole path had shipped, documented, and never once been run.
#
# Usage (see .github/workflows/ci.yml for the automated version):
#   cmake -S . -B build -DLWEH_BUILD_TESTS=OFF -DLWEH_BUILD_EXAMPLES=OFF -DLWEH_BUILD_SIZE_AUDIT=OFF
#   cmake --install build --prefix /some/prefix
#   cmake -S packaging_check -B packaging_check/build -DCMAKE_PREFIX_PATH=/some/prefix
#   cmake --build packaging_check/build
#   ./packaging_check/build/consumer   # must print OK and exit 0

cmake_minimum_required(VERSION 3.23)
project(LwEhPackagingCheck LANGUAGES CXX)

find_package(LwEh REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE LwEh::lw_eh)
