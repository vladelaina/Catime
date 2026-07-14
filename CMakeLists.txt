cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h" CATIME_VERSION_LINE
    REGEX "^#define[ \t]+CATIME_VERSION[ \t]+\"[0-9]+\\.[0-9]+\\.[0-9]+")
string(REGEX MATCH
    "\"([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[^\"]+)?\""
    CATIME_VERSION_MATCH "${CATIME_VERSION_LINE}")
if(NOT CATIME_VERSION_MATCH)
    message(FATAL_ERROR "CATIME_VERSION in resource/resource.h is invalid")
endif()

set(CATIME_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(CATIME_VERSION_MINOR "${CMAKE_MATCH_2}")
set(CATIME_VERSION_PATCH "${CMAKE_MATCH_3}")
set(CATIME_VERSION_BUILD 0)

# Project information
project(Catime VERSION
    "${CATIME_VERSION_MAJOR}.${CATIME_VERSION_MINOR}.${CATIME_VERSION_PATCH}"
    LANGUAGES C RC)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resource/catime_version_numeric.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/catime_version_numeric.h"
    @ONLY
)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(CATIME_ENABLE_WARNINGS "Enable stricter compiler warnings" ON)
option(CATIME_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CATIME_ENABLE_ASAN "Enable AddressSanitizer for analyzer builds" OFF)
option(CATIME_ENABLE_MSVC_ANALYZE "Enable MSVC static analysis" OFF)
option(CATIME_ENABLE_BINARY_HARDENING "Enable Win32 binary hardening flags" ON)
option(CATIME_ENABLE_WIN32_RESOURCES "Enable Win32 resource compilation" ON)
option(CATIME_VALIDATE_LANGUAGES "Validate language resource key alignment" ON)
option(CATIME_COMPACT_LANGUAGE_RESOURCES "Compact language resources when bundle compression is disabled" ON)
option(CATIME_COMPRESS_EMBEDDED_ASSETS "Compress embedded language and font resources" ON)
option(CATIME_AVOID_GNU_EMULATED_TLS "Use Win32 FLS with GNU POSIX thread-model compilers" ON)

# Set default build type to Release
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

# Collect all source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")
list(FILTER SOURCES EXCLUDE REGEX "[/\\\\]compressed_resource\\.c$")

# Collect all header files
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")

# Collect resource files
set(RESOURCE_FILES)
if(CATIME_ENABLE_WIN32_RESOURCES)
    file(GLOB LANGUAGE_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages/*.ini"
    )
    file(GLOB_RECURSE FONT_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/MIT/*.otf"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/MIT/*.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/OFL/*.otf"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/OFL/*.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/SIL/*.otf"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/font/SIL/*.ttf"
    )
    list(SORT LANGUAGE_RESOURCE_FILES)
    list(SORT FONT_RESOURCE_FILES)

    if(CATIME_VALIDATE_LANGUAGES OR CATIME_COMPACT_LANGUAGE_RESOURCES OR
       CATIME_COMPRESS_EMBEDDED_ASSETS)
        find_program(CATIME_NODE_EXECUTABLE NAMES node node.exe
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(NOT CATIME_NODE_EXECUTABLE)
            message(FATAL_ERROR
                "Node.js is required for language validation, compaction, or "
                "embedded resource compression. Install Node.js or disable the "
                "corresponding CATIME options."
            )
        endif()
    endif()

    if(CATIME_VALIDATE_LANGUAGES)
        add_custom_target(validate_languages
            COMMAND "${CATIME_NODE_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/i18n/validate_languages.js"
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/i18n/validate_languages.js"
                ${LANGUAGE_RESOURCE_FILES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Validating language resources"
            VERBATIM
        )
    endif()

    set(CATIME_LANGUAGE_RC "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages.rc")
    if(CATIME_COMPRESS_EMBEDDED_ASSETS)
        set(CATIME_EMBEDDED_ASSET_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/generated/assets"
        )
        set(CATIME_EMBEDDED_ASSET_BIN
            "${CATIME_EMBEDDED_ASSET_DIR}/catime-assets.bin"
        )
        set(CATIME_EMBEDDED_ASSET_RC
            "${CATIME_EMBEDDED_ASSET_DIR}/catime-assets.rc"
        )
        set(CATIME_EMBEDDED_ASSET_AUDIT
            "${CATIME_EMBEDDED_ASSET_DIR}/catime-assets.audit.json"
        )
        add_custom_target(prepare_embedded_assets
            BYPRODUCTS
                "${CATIME_EMBEDDED_ASSET_BIN}"
                "${CATIME_EMBEDDED_ASSET_RC}"
                "${CATIME_EMBEDDED_ASSET_AUDIT}"
            COMMAND "${CATIME_NODE_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_embedded_resources.js"
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource/embedded_assets.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
                    "${CATIME_EMBEDDED_ASSET_BIN}"
                    "${CATIME_EMBEDDED_ASSET_RC}"
                    "${CATIME_EMBEDDED_ASSET_AUDIT}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_embedded_resources.js"
                "${CMAKE_CURRENT_SOURCE_DIR}/resource/embedded_assets.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
                ${LANGUAGE_RESOURCE_FILES}
                ${FONT_RESOURCE_FILES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Compressing embedded language and font resources"
            VERBATIM
        )

        set(CATIME_LANGUAGE_RC "${CATIME_EMBEDDED_ASSET_RC}")
        set_source_files_properties("${CATIME_EMBEDDED_ASSET_RC}" PROPERTIES
            GENERATED TRUE
        )
    elseif(CATIME_COMPACT_LANGUAGE_RESOURCES)
        set(CATIME_EMBEDDED_LANGUAGE_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/embedded_languages"
        )
        set(CATIME_EMBEDDED_LANGUAGE_RC
            "${CMAKE_CURRENT_BINARY_DIR}/embedded_languages.rc"
        )
        set(CATIME_EMBEDDED_LANGUAGE_FILES)
        foreach(LANGUAGE_RESOURCE_FILE IN LISTS LANGUAGE_RESOURCE_FILES)
            get_filename_component(LANGUAGE_RESOURCE_NAME
                "${LANGUAGE_RESOURCE_FILE}" NAME
            )
            list(APPEND CATIME_EMBEDDED_LANGUAGE_FILES
                "${CATIME_EMBEDDED_LANGUAGE_DIR}/${LANGUAGE_RESOURCE_NAME}"
            )
        endforeach()

        add_custom_command(
            OUTPUT
                "${CATIME_EMBEDDED_LANGUAGE_RC}"
                ${CATIME_EMBEDDED_LANGUAGE_FILES}
            COMMAND "${CATIME_NODE_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/i18n/prepare_embedded_languages.js"
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages.rc"
                    "${CATIME_EMBEDDED_LANGUAGE_DIR}"
                    "${CATIME_EMBEDDED_LANGUAGE_RC}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/i18n/prepare_embedded_languages.js"
                "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages.rc"
                ${LANGUAGE_RESOURCE_FILES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Preparing compact embedded language resources"
            VERBATIM
        )

        set(CATIME_LANGUAGE_RC "${CATIME_EMBEDDED_LANGUAGE_RC}")
        set_source_files_properties("${CATIME_EMBEDDED_LANGUAGE_RC}" PROPERTIES
            GENERATED TRUE
            OBJECT_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h;${CATIME_EMBEDDED_LANGUAGE_FILES}"
        )
    else()
        set_source_files_properties("${CATIME_LANGUAGE_RC}" PROPERTIES
            OBJECT_DEPENDS
                "${LANGUAGE_RESOURCE_FILES};${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
        )
    endif()

    set(RESOURCE_FILES
        resource/resource.rc
        "${CATIME_LANGUAGE_RC}"
        resource/catime.rc
    )

    file(GLOB DIALOG_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/*_dialog.rc"
    )
    set(RESOURCE_RC_DEPENDENCIES
        ${DIALOG_RESOURCE_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/app.manifest"
        "${CMAKE_CURRENT_SOURCE_DIR}/asset/icon/catime.ico"
    )
    if(NOT CATIME_COMPRESS_EMBEDDED_ASSETS)
        list(APPEND RESOURCE_RC_DEPENDENCIES ${FONT_RESOURCE_FILES})
    endif()
    set_source_files_properties(resource/resource.rc PROPERTIES
        OBJECT_DEPENDS "${RESOURCE_RC_DEPENDENCIES}"
    )
    set_source_files_properties(resource/catime.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
    )
endif()

# Create executable
add_executable(catime ${SOURCES} ${HEADERS} ${RESOURCE_FILES})

include(CTest)
if(BUILD_TESTING)
    add_executable(window_placement_tests
        tests/window_placement_tests.c
        src/window/window_placement.c
    )
    target_include_directories(window_placement_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    add_test(NAME window_placement COMMAND window_placement_tests)
endif()

# Recent MinGW GCC distributions automatically append default-manifest.o,
# which conflicts with Catime's complete application manifest. Shadow that
# startup object with an empty linker script; app.manifest carries the retained
# longPathAware declaration explicitly.
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
   CATIME_ENABLE_WIN32_RESOURCES)
    set(CATIME_EMPTY_MANIFEST_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/generated/no-default-manifest"
    )
    file(MAKE_DIRECTORY "${CATIME_EMPTY_MANIFEST_DIR}")
    file(WRITE "${CATIME_EMPTY_MANIFEST_DIR}/default-manifest.o"
        "/* Intentionally empty linker script. */\n"
    )
    target_link_options(catime PRIVATE "-B${CATIME_EMPTY_MANIFEST_DIR}/")
endif()

if(CATIME_ENABLE_WIN32_RESOURCES AND CATIME_COMPRESS_EMBEDDED_ASSETS)
    target_sources(catime PRIVATE
        src/utils/compressed_resource.c
        libs/miniz/miniz_tinfl.c
    )
endif()

if(TARGET validate_languages)
    add_dependencies(catime validate_languages)
endif()
if(TARGET prepare_embedded_assets)
    add_dependencies(catime prepare_embedded_assets)
endif()

# Add include directories
target_include_directories(catime PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    include
    src
    libs/miniaudio
)

if(CATIME_ENABLE_WIN32_RESOURCES AND CATIME_COMPRESS_EMBEDDED_ASSETS)
    target_include_directories(catime PRIVATE
        "${CATIME_EMBEDDED_ASSET_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/resource"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/miniz"
    )
endif()

# Add compile definitions
target_compile_definitions(catime PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
    $<$<COMPILE_LANGUAGE:C>:MINIAUDIO_IMPLEMENTATION>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_GENERATION>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_ENCODING>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_VORBIS>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_OPUS>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_FLAC>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_RESOURCE_MANAGER>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_NODE_GRAPH>
    $<$<COMPILE_LANGUAGE:C>:MA_NO_ENGINE>
    $<$<COMPILE_LANGUAGE:C>:MA_ENABLE_ONLY_SPECIFIC_BACKENDS>
    $<$<COMPILE_LANGUAGE:C>:MA_ENABLE_WASAPI>
    $<$<COMPILE_LANGUAGE:C>:MA_ENABLE_DSOUND>
    $<$<COMPILE_LANGUAGE:C>:MA_ENABLE_WINMM>
)

if(CATIME_ENABLE_WIN32_RESOURCES AND CATIME_COMPRESS_EMBEDDED_ASSETS)
    target_compile_definitions(catime PRIVATE
        CATIME_COMPRESSED_EMBEDDED_RESOURCES=1
        $<$<COMPILE_LANGUAGE:C>:MINIZ_NO_STDIO>
        $<$<COMPILE_LANGUAGE:C>:MINIZ_NO_TIME>
        $<$<COMPILE_LANGUAGE:C>:MINIZ_NO_MALLOC>
    )
endif()

if(CATIME_USE_WIN32_FLS)
    target_compile_definitions(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:CATIME_USE_WIN32_FLS=1>
    )
endif()

if(MSVC)
    target_compile_definitions(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:_CRT_SECURE_NO_WARNINGS>
        $<$<COMPILE_LANGUAGE:C>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<COMPILE_LANGUAGE:C>:strcasecmp=_stricmp>
        $<$<COMPILE_LANGUAGE:C>:strdup=_strdup>
    )
endif()

if(MSVC)
    target_compile_options(catime PRIVATE $<$<COMPILE_LANGUAGE:C>:/std:c11>)
endif()

if(CATIME_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(catime PRIVATE $<$<COMPILE_LANGUAGE:C>:/W4>)
        if(CATIME_WARNINGS_AS_ERRORS)
            target_compile_options(catime PRIVATE $<$<COMPILE_LANGUAGE:C>:/WX>)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Wall>
            $<$<COMPILE_LANGUAGE:C>:-Wextra>
            $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
        )
        if(CATIME_WARNINGS_AS_ERRORS)
            target_compile_options(catime PRIVATE $<$<COMPILE_LANGUAGE:C>:-Werror>)
        endif()
    endif()
endif()

# Set compiler flags based on build type
if(MSVC)
    target_link_options(catime PRIVATE /SUBSYSTEM:WINDOWS)

    if(CATIME_ENABLE_BINARY_HARDENING AND NOT CATIME_ENABLE_ASAN)
        target_compile_options(catime PRIVATE $<$<COMPILE_LANGUAGE:C>:/guard:cf>)
        target_link_options(catime PRIVATE /DYNAMICBASE /NXCOMPAT /guard:cf)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            target_link_options(catime PRIVATE /SAFESEH)
        endif()
    endif()

    if(CATIME_ENABLE_MSVC_ANALYZE)
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:/analyze>
            $<$<COMPILE_LANGUAGE:C>:/analyze:quiet>
            $<$<COMPILE_LANGUAGE:C>:/analyze:autolog>
            $<$<COMPILE_LANGUAGE:C>:/analyze:log:format:sarif>
        )
    endif()

    if(CATIME_ENABLE_ASAN)
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:/fsanitize=address>
            $<$<COMPILE_LANGUAGE:C>:/Zi>
            $<$<COMPILE_LANGUAGE:C>:/Od>
        )
        target_link_options(catime PRIVATE /fsanitize=address /DEBUG /INCREMENTAL:NO)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(catime PRIVATE NDEBUG)
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:/O2>
            $<$<COMPILE_LANGUAGE:C>:/Oi>
            $<$<COMPILE_LANGUAGE:C>:/Gy>
        )
        target_link_options(catime PRIVATE /OPT:REF /OPT:ICF)
    else()
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:/Zi>
            $<$<COMPILE_LANGUAGE:C>:/Od>
        )
        target_link_options(catime PRIVATE /DEBUG)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(catime PRIVATE NDEBUG)

    set(CATIME_SIZE_OPTIMIZATION_FLAG -Os)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-Oz" CATIME_COMPILER_SUPPORTS_OZ)
        if(CATIME_COMPILER_SUPPORTS_OZ)
            set(CATIME_SIZE_OPTIMIZATION_FLAG -Oz)
        endif()
    endif()

    # Optimization flags for MinGW (size-optimized)
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${CATIME_SIZE_OPTIMIZATION_FLAG}>
        $<$<COMPILE_LANGUAGE:C>:-mtune=generic>
        $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:C>:-fno-strict-aliasing>
        $<$<COMPILE_LANGUAGE:C>:-flto>
        $<$<COMPILE_LANGUAGE:C>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:C>:-fomit-frame-pointer>
        $<$<COMPILE_LANGUAGE:C>:-fmerge-all-constants>
        $<$<COMPILE_LANGUAGE:C>:-fno-math-errno>
        $<$<COMPILE_LANGUAGE:C>:-fno-trapping-math>
        $<$<COMPILE_LANGUAGE:C>:-ffast-math>
        $<$<COMPILE_LANGUAGE:C>:-finline-small-functions>
        $<$<COMPILE_LANGUAGE:C>:-finline-functions-called-once>
        $<$<COMPILE_LANGUAGE:C>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:C>:-fno-asynchronous-unwind-tables>
        $<$<COMPILE_LANGUAGE:C>:-fno-ident>
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unknown-warning-option>
    )

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CATIME_DISCARD_EH_FRAME_SCRIPT
            "${CMAKE_CURRENT_BINARY_DIR}/generated/discard-eh-frame.ld"
        )
        file(WRITE "${CATIME_DISCARD_EH_FRAME_SCRIPT}"
            "SECTIONS\n{\n  /DISCARD/ : { *(.eh_frame*) }\n}\nINSERT AFTER .data;\n"
        )

        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fipa-pta>
            $<$<COMPILE_LANGUAGE:C>:-fwhole-program>
            $<$<COMPILE_LANGUAGE:C>:-flto-partition=none>
        )
        target_link_options(catime PRIVATE
            -fipa-pta
            -fwhole-program
            -flto-partition=none
            "LINKER:-T,${CATIME_DISCARD_EH_FRAME_SCRIPT}"
        )
    endif()

    # Link-time optimization and size reduction
    target_link_options(catime PRIVATE
        -mwindows                                # Windows GUI application
        -flto                                    # Link-time optimization
        -Wl,--gc-sections                        # Remove unused sections
        -Wl,--strip-all                          # Strip all symbols
        -Wl,--build-id=none                      # Remove build ID section
        -Wl,--no-insert-timestamp                # Remove PE timestamp for reproducibility
        -static                                  # Static link CRT
    )
else()
    # Debug flags
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-g>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unknown-warning-option>
    )
    
    target_link_options(catime PRIVATE
        -mwindows
        -static
    )
endif()

# Link Windows libraries
target_link_libraries(catime PRIVATE
    ole32
    shell32
    comdlg32
    uuid
    wininet
    winmm
    comctl32
    dwmapi
    user32
    gdi32
    shlwapi
    advapi32
    powrprof
    iphlpapi
    windowscodecs
    propsys
    crypt32
)

# Set output directory - use custom output dir if specified
if(DEFINED CATIME_OUTPUT_DIR)
    set(OUTPUT_DIR ${CATIME_OUTPUT_DIR})
else()
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()

set_target_properties(catime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME "catime"
)

# Custom target to display build information
add_custom_command(TARGET catime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build completed successfully!"
    COMMAND ${CMAKE_COMMAND} -E echo "Output directory: ${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Executable: $<TARGET_FILE:catime>"
)

# Option for debug mode
option(ENABLE_DEBUG "Enable debug mode" OFF)
if(ENABLE_DEBUG)
    target_compile_definitions(catime PRIVATE DEBUG_MODE)
endif()
