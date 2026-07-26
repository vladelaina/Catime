# Enforce a reviewable size for first-party C source and header files.
set(CATIME_MAX_FIRST_PARTY_SOURCE_LINES 300)

# Remove an entry as soon as its module is split below the line limit.
set(_catime_line_limit_allowlist)

set(_catime_first_party_code)
foreach(_catime_root IN ITEMS src include tests)
    file(GLOB_RECURSE _catime_root_code
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_catime_root}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_catime_root}/*.h"
    )
    list(APPEND _catime_first_party_code ${_catime_root_code})
endforeach()

set(_catime_line_limit_violations)
set(_catime_active_allowlist)
foreach(_catime_file IN LISTS _catime_first_party_code)
    file(READ "${_catime_file}" _catime_contents)
    string(REGEX REPLACE "[^\n]" "" _catime_newlines "${_catime_contents}")
    string(LENGTH "${_catime_newlines}" _catime_line_count)
    if(NOT _catime_contents STREQUAL "" AND
       NOT _catime_contents MATCHES "\n$")
        math(EXPR _catime_line_count "${_catime_line_count} + 1")
    endif()

    file(RELATIVE_PATH _catime_relative
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${_catime_file}"
    )
    string(REPLACE "\\" "/" _catime_relative "${_catime_relative}")

    if(_catime_line_count GREATER CATIME_MAX_FIRST_PARTY_SOURCE_LINES)
        if(_catime_relative IN_LIST _catime_line_limit_allowlist)
            list(APPEND _catime_active_allowlist "${_catime_relative}")
        else()
            list(APPEND _catime_line_limit_violations
                "${_catime_relative} (${_catime_line_count} lines)"
            )
        endif()
    endif()
endforeach()

set(_catime_stale_allowlist)
foreach(_catime_relative IN LISTS _catime_line_limit_allowlist)
    if(NOT _catime_relative IN_LIST _catime_active_allowlist)
        list(APPEND _catime_stale_allowlist "${_catime_relative}")
    endif()
endforeach()

if(_catime_line_limit_violations)
    string(JOIN "\n  " _catime_line_limit_text
        ${_catime_line_limit_violations}
    )
    message(FATAL_ERROR
        "First-party C files must not exceed "
        "${CATIME_MAX_FIRST_PARTY_SOURCE_LINES} lines:\n"
        "  ${_catime_line_limit_text}"
    )
endif()

if(_catime_stale_allowlist)
    string(JOIN "\n  " _catime_stale_allowlist_text
        ${_catime_stale_allowlist}
    )
    message(FATAL_ERROR
        "Remove resolved files from _catime_line_limit_allowlist:\n"
        "  ${_catime_stale_allowlist_text}"
    )
endif()

unset(_catime_line_limit_allowlist)
unset(_catime_first_party_code)
unset(_catime_root_code)
unset(_catime_line_limit_violations)
unset(_catime_active_allowlist)
unset(_catime_stale_allowlist)
unset(_catime_contents)
unset(_catime_newlines)
unset(_catime_line_count)
unset(_catime_relative)
unset(_catime_file)
unset(_catime_root)
unset(_catime_line_limit_text)
unset(_catime_stale_allowlist_text)
