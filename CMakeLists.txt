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

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Collect all source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")

# Collect all header files
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")

# Collect resource files
set(RESOURCE_FILES)
if(CATIME_ENABLE_WIN32_RESOURCES)
    set(RESOURCE_FILES
        resource/resource.rc
        resource/languages.rc
        resource/catime.rc
    )

    file(GLOB LANGUAGE_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/languages/*.ini"
    )
    set(LANGUAGE_RC_DEPENDENCIES
        ${LANGUAGE_RESOURCE_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
    )
    set_source_files_properties(resource/languages.rc PROPERTIES
        OBJECT_DEPENDS "${LANGUAGE_RC_DEPENDENCIES}"
    )

    if(CATIME_VALIDATE_LANGUAGES)
        find_program(CATIME_NODE_EXECUTABLE NAMES node node.exe)
        if(CATIME_NODE_EXECUTABLE)
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
        else()
            message(FATAL_ERROR
                "Node.js is required for language resource validation. "
                "Install Node.js or configure with -DCATIME_VALIDATE_LANGUAGES=OFF."
            )
        endif()
    endif()

    file(GLOB DIALOG_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/*_dialog.rc"
    )
    set(RESOURCE_RC_DEPENDENCIES
        ${DIALOG_RESOURCE_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/resource/app.manifest"
    )
    set_source_files_properties(resource/resource.rc PROPERTIES
        OBJECT_DEPENDS "${RESOURCE_RC_DEPENDENCIES}"
    )
    set_source_files_properties(resource/catime.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resource/resource.h"
    )
endif()

# Create executable
add_executable(catime ${SOURCES} ${HEADERS} ${RESOURCE_FILES})

if(TARGET validate_languages)
    add_dependencies(catime validate_languages)
endif()

# Add include directories
target_include_directories(catime PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    include
    src
    libs/miniaudio
)

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
)

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

    # Optimization flags for MinGW (size-optimized)
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Os>
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
