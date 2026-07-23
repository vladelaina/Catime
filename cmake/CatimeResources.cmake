list(FILTER SOURCES EXCLUDE REGEX "[/\\\\]compressed_resource\\.c$")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")

set(RESOURCE_FILES)
if(NOT CATIME_ENABLE_WIN32_RESOURCES)
    return()
endif()

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
