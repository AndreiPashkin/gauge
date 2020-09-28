# Project's C++ extension build configuration.
cmake_minimum_required(VERSION 3.15)
project(gauge)

include(FetchContent)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FETCHCONTENT_QUIET OFF)

include(FetchDependencies.cmake)

# Add Boost library.
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

set(BOOST_ROOT ${THIRD_PARTY}/boost)
find_package(Boost 1.73.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost is not found.")
endif()

# Add pybind11 library.
add_subdirectory(${THIRD_PARTY}/pybind11)

# Add Fmt library.
add_subdirectory(${THIRD_PARTY}/fmt)
set_target_properties(fmt PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Add Spdlog library.
set(SPDLOG_FMT_EXTERNAL_HO ON)
set(SPDLOG_BUILD_SHARED OFF)
add_subdirectory(${THIRD_PARTY}/spdlog)
set_target_properties(spdlog PROPERTIES EXCLUDE_FROM_ALL TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(NOTICE "Log level is set to 'DEBUG' due to release build type.")
    target_compile_definitions(
        spdlog INTERFACE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
    )
else()
    message(
        NOTICE
        "Log level is set to 'TRACE' due to non-release build type."
    )
    target_compile_definitions(
        spdlog INTERFACE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    )
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Add project's public headers location.
include_directories(include)
# Add project's private headers location.
include_directories(src/cpp/)

file(GLOB GAUGE_SOURCES "src/cpp/gauge/*.cpp" "src/cpp/gauge/utils/*.cpp")

# Declare main target as per pybind11 docs: https://git.io/JfTfU.
pybind11_add_module(_gauge ${GAUGE_SOURCES} SYSTEM)
target_link_libraries(_gauge PRIVATE fmt::fmt)
target_link_libraries(_gauge PRIVATE spdlog::spdlog)
target_link_libraries(_gauge PRIVATE Boost::boost)

file(GLOB_RECURSE GAUGE_SOURCES_AND_HEADERS include/* src/cpp/*)

add_custom_target(
    clang-format
    COMMAND scripts/format.py ${GAUGE_SOURCES_AND_HEADERS}
)
add_custom_target(
    clang-format-check
    COMMAND scripts/format.py --check ${GAUGE_SOURCES_AND_HEADERS}
)
