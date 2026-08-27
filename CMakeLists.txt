cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
    CATIME_VERSION_LINE
    REGEX "^#define[ \t]+CATIME_VERSION[ \t]+\"[0-9]+\\.[0-9]+\\.[0-9]+"
)
string(REGEX MATCH
    "\"([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[^\"]+)?\""
    CATIME_VERSION_MATCH "${CATIME_VERSION_LINE}"
)
if(NOT CATIME_VERSION_MATCH)
    message(FATAL_ERROR "CATIME_VERSION in resource/resource.h is invalid")
endif()

set(CATIME_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(CATIME_VERSION_MINOR "${CMAKE_MATCH_2}")
set(CATIME_VERSION_PATCH "${CMAKE_MATCH_3}")
set(CATIME_VERSION_BUILD 0)
project(Catime VERSION
    "${CATIME_VERSION_MAJOR}.${CATIME_VERSION_MINOR}.${CATIME_VERSION_PATCH}"
    LANGUAGES C RC
)
include(cmake/CatimePureC.cmake)
include(cmake/CatimeSourceLayout.cmake)
include(cmake/CatimeInteractionSafety.cmake)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resource/catime_version_numeric.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/catime_version_numeric.h"
    @ONLY
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Catime supports Windows 7. Keep the SDK target explicit so changing the
# compiler or Windows SDK cannot silently expose APIs that would prevent the
# executable from starting on Windows 7.
if(WIN32)
    add_compile_definitions(
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000
    )
endif()

option(CATIME_ENABLE_WARNINGS "Enable stricter compiler warnings" ON)
option(CATIME_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CATIME_ENABLE_ASAN "Enable AddressSanitizer for analyzer builds" OFF)
option(CATIME_ENABLE_MSVC_ANALYZE "Enable MSVC static analysis" OFF)
option(CATIME_ENABLE_BINARY_HARDENING "Enable Win32 binary hardening flags" ON)
set(CATIME_RELEASE_OPTIMIZATION "-Os" CACHE STRING
    "GNU/Clang optimization level for Release builds (-Os or -Oz)"
)
set_property(CACHE CATIME_RELEASE_OPTIMIZATION PROPERTY STRINGS "-Os" "-Oz")
if(NOT CATIME_RELEASE_OPTIMIZATION MATCHES "^-O[sz]$")
    message(FATAL_ERROR
        "CATIME_RELEASE_OPTIMIZATION must be -Os or -Oz"
    )
endif()
option(CATIME_ENABLE_WIN32_RESOURCES "Enable Win32 resource compilation" ON)
option(CATIME_VALIDATE_LANGUAGES "Validate language resource key alignment" ON)
option(CATIME_COMPACT_LANGUAGE_RESOURCES
    "Compact language resources when bundle compression is disabled" ON)
option(CATIME_COMPRESS_EMBEDDED_ASSETS
    "Compress embedded language and font resources" OFF)
option(CATIME_AVOID_GNU_EMULATED_TLS
    "Use Win32 FLS with GNU POSIX thread-model compilers" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CATIME_USE_WIN32_FLS OFF)
if(CATIME_AVOID_GNU_EMULATED_TLS AND WIN32 AND
   CMAKE_C_COMPILER_ID STREQUAL "GNU")
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -v
        RESULT_VARIABLE CATIME_COMPILER_INFO_RESULT
        OUTPUT_VARIABLE CATIME_COMPILER_INFO_STDOUT
        ERROR_VARIABLE CATIME_COMPILER_INFO_STDERR
    )
    string(CONCAT CATIME_COMPILER_INFO
        "${CATIME_COMPILER_INFO_STDOUT}" "${CATIME_COMPILER_INFO_STDERR}"
    )
    if(CATIME_COMPILER_INFO_RESULT EQUAL 0 AND
       CATIME_COMPILER_INFO MATCHES "Thread model:[ \t]*posix")
        set(CATIME_USE_WIN32_FLS ON)
        message(STATUS "Using Win32 FLS instead of GNU emulated TLS")
    endif()
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")
include(cmake/CatimeResources.cmake)

add_executable(catime ${SOURCES} ${HEADERS} ${RESOURCE_FILES})
add_dependencies(catime catime_interaction_safety_check)
include(cmake/CatimeTests.cmake)
include(cmake/CatimeTarget.cmake)
include(cmake/CatimeCompiler.cmake)
include(cmake/CatimeOutput.cmake)
