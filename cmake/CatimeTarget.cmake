# MinGW may append default-manifest.o, which conflicts with the application
# manifest. Shadow it with an empty linker script.
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

# The application manifest is compiled into resource/resource.rc. Prevent
# MSVC from generating a second RT_MANIFEST resource at link time.
if(MSVC AND CATIME_ENABLE_WIN32_RESOURCES)
    target_link_options(catime PRIVATE /MANIFEST:NO)
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
