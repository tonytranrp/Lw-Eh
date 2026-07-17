cmake_minimum_required(VERSION 3.23) # target_sources(... FILE_SET HEADERS)
project(LwEh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # lets clangd use real per-target flags/includes (see .clangd)

add_library(lw_eh INTERFACE)
add_library(LwEh::lw_eh ALIAS lw_eh)

target_sources(lw_eh INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        include/lweh/lweh.hpp
        include/lweh/config.hpp
        include/lweh/delegate.hpp
        include/lweh/signal.hpp
        include/lweh/intrusive_signal.hpp
        include/lweh/detail/signal_core.hpp
        include/lweh/detail/intrusive_core.hpp
)
target_compile_features(lw_eh INTERFACE cxx_std_17)

include(cmake/LwEhAggressiveFlags.cmake)

option(LWEH_BUILD_TESTS      "Build the host CTest correctness suite"          ${PROJECT_IS_TOP_LEVEL})
option(LWEH_BUILD_EXAMPLES   "Build the cross-compiled embedded examples"      OFF)
option(LWEH_BUILD_SIZE_AUDIT "Build the size-audit control/with-lweh binaries" OFF)
option(LWEH_SANITIZE         "Build tests/ with -fsanitize=address,undefined (host-only; never applied to lw_eh_size_flags, which examples/ cross-compile with -- see tests/CMakeLists.txt)" OFF)

if(LWEH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LWEH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LWEH_BUILD_SIZE_AUDIT)
    add_subdirectory(size_audit)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS lw_eh EXPORT LwEhTargets FILE_SET HEADERS)
install(EXPORT LwEhTargets NAMESPACE LwEh:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LwEh)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LwEhConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LwEhConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LwEh
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/LwEhConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/LwEhConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/LwEhConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LwEh
)

# No find_package / FetchContent / ExternalProject_Add / vcpkg / Conan calls
# anywhere in this file or anywhere else in the repo, by design (research.md §B1).
