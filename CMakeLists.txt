# Project's C++ extension build configuration.
cmake_minimum_required(VERSION 3.15)
project(gauge)

include(FetchContent)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(FETCHCONTENT_QUIET OFF)
set(THIRD_PARTY ${CMAKE_SOURCE_DIR}/thirdparty)

file(MAKE_DIRECTORY ${THIRD_PARTY})

if(NOT FETCH_THIRDPARTY_SOURCES)
    set(FETCH_THIRDPARTY_SOURCES FALSE)
endif()

# Add Boost library.
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
# TODO: Can it be made partial?
if(FETCH_THIRDPARTY_SOURCES)
    FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG        boost-1.73.0
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        SOURCE_DIR     ${THIRD_PARTY}/boost
    )
    FetchContent_GetProperties(boost)
    if(NOT boost_POPULATED)
        FetchContent_Populate(boost)
    endif()
    if(UNIX)
        execute_process (
            COMMAND ./bootstrap.sh
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        if(NOT result EQUAL 0)
            message( FATAL_ERROR "Failed to build Boost: ${result}")
        endif()
        execute_process (
            COMMAND ./b2 headers
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        if(NOT result EQUAL 0)
            message( FATAL_ERROR "Failed to build Boost: ${result}")
        endif()
    elseif(WIN32)
        execute_process (
            COMMAND ./bootstrap.bat
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        execute_process (
            COMMAND b2 headers
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
    endif()
endif()

set(BOOST_ROOT ${THIRD_PARTY}/boost)
find_package(Boost 1.73.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost is not found.")
endif()

# Add pybind11 library.
if(FETCH_THIRDPARTY_SOURCES)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.5.0
        GIT_PROGRESS   TRUE
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${THIRD_PARTY}/pybind11
    )
    if(NOT pybind11_POPULATED)
        FetchContent_Populate(pybind11)
    endif()
endif(FETCH_THIRDPARTY_SOURCES)
add_subdirectory(${THIRD_PARTY}/pybind11)

# Add Fmt library.
if(FETCH_THIRDPARTY_SOURCES)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        6.2.1
        GIT_PROGRESS   TRUE
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${THIRD_PARTY}/fmt
    )
    if(NOT fmt_POPULATED)
        FetchContent_Populate(fmt)
    endif()
endif()
add_subdirectory(${THIRD_PARTY}/fmt)
set_target_properties(fmt PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Add Spdlog library.
set(SPDLOG_FMT_EXTERNAL_HO ON)
set(SPDLOG_BUILD_SHARED OFF)
if(FETCH_THIRDPARTY_SOURCES)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.6.0
        GIT_PROGRESS   TRUE
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${THIRD_PARTY}/spdlog
    )
    if(NOT spdlog_POPULATED)
        FetchContent_Populate(spdlog)
    endif()
endif()
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
pybind11_add_module(_gauge ${GAUGE_SOURCES})
target_link_libraries(_gauge PRIVATE fmt::fmt)
target_link_libraries(_gauge PRIVATE spdlog::spdlog)
target_link_libraries(_gauge PRIVATE Boost::boost)

unset(FETCH_THIRDPARTY_SOURCES CACHE)
