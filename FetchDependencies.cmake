file(MAKE_DIRECTORY ${THIRD_PARTY})

# Fetch Boost.
# TODO: Can it be made partial?
if(NOT EXISTS "${BOOST_URL}")
    message(NOTICE "Fetching Boost...")
    if(UNIX)
        set(
            BOOST_URL
            https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz
        )
        set(BOOST_ARCHIVE "${CMAKE_BINARY_DIR}/boost.tar.gz")
        set(BOOST_ARCHIVE_SHA256 aeb26f80e80945e82ee93e5939baebdca47b9dee80a07d3144be1e1a6a66dd6a)
    elseif(WIN32)
        set(
            BOOST_URL
            https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.zip
        )
        set(BOOST_ARCHIVE "${CMAKE_BINARY_DIR}/boost.zip")
        set(BOOST_ARCHIVE_SHA256 caf36d7c13b3d8ce62282a64a695113945a13b0f1796a45160726d04295f95ed)
    else()
        message(FATAL_ERROR "Platform is not supported.")
    endif()
    file(
        DOWNLOAD ${BOOST_URL} ${BOOST_ARCHIVE}
        STATUS result
        SHOW_PROGRESS
        EXPECTED_HASH SHA256=${BOOST_ARCHIVE_SHA256}
    )
    set(BOOST_TEMP "${CMAKE_BINARY_DIR}/boost")
    file(
        ARCHIVE_EXTRACT INPUT ${BOOST_ARCHIVE}
        DESTINATION ${BOOST_TEMP}
    )
    file(GLOB result RELATIVE ${BOOST_TEMP} "${BOOST_TEMP}/*")
    file(RENAME "${BOOST_TEMP}/${result}" ${BOOST_ROOT})
    file(REMOVE_RECURSE ${BOOST_TEMP})
endif()

# Fetch Pybind11.
if(NOT EXISTS "${THIRD_PARTY}/pybind11/")
    message(NOTICE "Fetching Pybind11...")
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.6.1
        GIT_PROGRESS   TRUE
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${THIRD_PARTY}/pybind11
    )
    if(NOT pybind11_POPULATED)
        FetchContent_Populate(pybind11)
    endif()
endif()

# Fetch Fmt library.
if(NOT EXISTS "${THIRD_PARTY}/fmt/")
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
if(NOT EXISTS "${THIRD_PARTY}/spdlog/")
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
