set(THIRD_PARTY ${CMAKE_SOURCE_DIR}/thirdparty)
file(MAKE_DIRECTORY ${THIRD_PARTY})

if(NOT FETCH_DEPENDENCIES)
    set(FETCH_DEPENDENCIES FALSE)
endif()

FILE(GLOB deps "${THIRD_PARTY}/*")
if(NOT deps)
    set(FETCH_DEPENDENCIES TRUE)
endif()

# Fetch Boost.
# TODO: Can it be made partial?
if(FETCH_DEPENDENCIES)
    message(NOTICE "Fetching Boost...")
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

# Fetch Pybind11.
if(FETCH_DEPENDENCIES)
    message(NOTICE "Fetching Pybind11...")
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
endif(FETCH_DEPENDENCIES)

# Fetch Fmt library.
if(FETCH_DEPENDENCIES)
    message(NOTICE "Fetching Fmt...")
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

# Fetch Spdlog.
if(FETCH_DEPENDENCIES)
    message(NOTICE "Fetching Spdlog...")
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

unset(FETCH_DEPENDENCIES CACHE)
