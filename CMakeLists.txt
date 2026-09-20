cmake_minimum_required(VERSION 3.14)

# Pangolin is a header-only library, so this file compiles nothing. It exists to
# expose the headers as the imported target pgl::pgl, to install them, and to
# generate the package config that makes find_package(pgl) work. Consumers who
# prefer to pass -Iinclude by hand need none of it.
project(pgl
        VERSION 1.0.0
        DESCRIPTION "Header-only C++20 library for plane computational geometry"
        HOMEPAGE_URL "https://github.com/gfonsecabr/pgl"
        LANGUAGES CXX)

# Keep the VERSION above in step with the release tag. The generated
# pgl-config-version.cmake is SameMajorVersion, so find_package(pgl 1.0)
# is satisfied by any later 1.x.

include(GNUInstallDirs)

# PROJECT_IS_TOP_LEVEL is CMake 3.21; this repo still supports 3.14 consumers.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PGL_IS_TOP_LEVEL YES)
else()
    set(PGL_IS_TOP_LEVEL NO)
endif()

option(PGL_INSTALL "Generate install rules and the find_package(pgl) config" ${PGL_IS_TOP_LEVEL})
option(PGL_BUILD_EXAMPLES "Build the programs in examples/" OFF)

add_library(pgl INTERFACE)
add_library(pgl::pgl ALIAS pgl)

target_compile_features(pgl INTERFACE cxx_std_20)

# Every header includes its siblings relative to the include root ("pgl.hpp",
# "core/rational.hpp", ...), so the installed copy keeps that root intact: the
# headers go under <prefix>/include/pgl rather than spilling core/, shape/ and
# implementation/ into <prefix>/include. Both spellings below therefore resolve
# a user's #include "pgl.hpp" the same way in the build tree and once installed.
target_include_directories(pgl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pgl>)

# The library is dependency-free wherever the compiler has a native 128-bit
# integer. Where it does not (MSVC), core/numeric.hpp falls back to
# Boost.Multiprecision for pgl::int128, so Boost's headers are needed to compile
# against pgl at all. Which branch applies is a property of the compiler in use,
# which is why this is tested here rather than recorded once in the exported
# target -- cmake/pgl-config.cmake.in repeats the check for installed consumers.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#if !defined(__SIZEOF_INT128__)
#error no native 128-bit integer
#endif
int main() { return 0; }
" PGL_HAVE_INT128)

if(NOT PGL_HAVE_INT128)
    # CONFIG mode first: CMake 4 removed the FindBoost module (policy CMP0167),
    # and every Boost since 1.70 ships a config of its own. Module mode stays as
    # the fallback for an older Boost under a CMake that still provides it.
    find_package(Boost QUIET CONFIG)
    if(NOT Boost_FOUND AND CMAKE_VERSION VERSION_LESS 3.30)
        find_package(Boost QUIET MODULE)
    endif()
    if(TARGET Boost::headers)
        target_link_libraries(pgl INTERFACE Boost::headers)
    elseif(TARGET Boost::boost)
        target_link_libraries(pgl INTERFACE Boost::boost)
    else()
        message(FATAL_ERROR
            "${CMAKE_CXX_COMPILER_ID} provides no native 128-bit integer, so pgl "
            "needs the Boost.Multiprecision headers for pgl::int128, and Boost "
            "was not found. Install Boost (headers only) or point CMake at it "
            "with -DBoost_ROOT=<path>.")
    endif()
endif()

if(PGL_BUILD_EXAMPLES)
    # Target names are prefixed so that they cannot collide with a parent
    # project's; the binaries keep the plain names examples/Makefile produces.
    file(GLOB PGL_EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    foreach(pgl_example_source IN LISTS PGL_EXAMPLE_SOURCES)
        get_filename_component(pgl_example_name "${pgl_example_source}" NAME_WE)
        add_executable(pgl_${pgl_example_name} "${pgl_example_source}")
        target_link_libraries(pgl_${pgl_example_name} PRIVATE pgl::pgl)
        set_target_properties(pgl_${pgl_example_name} PROPERTIES OUTPUT_NAME ${pgl_example_name})
    endforeach()
endif()

if(PGL_INSTALL)
    include(CMakePackageConfigHelpers)

    set(PGL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/pgl")

    install(TARGETS pgl EXPORT pglTargets)

    # FILES_MATCHING keeps the maintainer scripts that live beside the headers
    # (implementation/split_predicates.py) out of the installed tree.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/pgl"
            FILES_MATCHING PATTERN "*.hpp")

    install(EXPORT pglTargets
            FILE pgl-targets.cmake
            NAMESPACE pgl::
            DESTINATION "${PGL_INSTALL_CMAKEDIR}")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pgl-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/pgl-config.cmake"
        INSTALL_DESTINATION "${PGL_INSTALL_CMAKEDIR}")

    # ARCH_INDEPENDENT: there is no compiled artifact, so a 32/64-bit difference
    # between the installing and the consuming build must not reject the package.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/pgl-config-version.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/pgl-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/pgl-config-version.cmake"
            DESTINATION "${PGL_INSTALL_CMAKEDIR}")
endif()
