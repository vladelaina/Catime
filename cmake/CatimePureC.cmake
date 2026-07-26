# Keep first-party implementation code in standard C translation units.
get_property(_catime_enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
if("CXX" IN_LIST _catime_enabled_languages)
    message(FATAL_ERROR
        "Catime is a C-only project; the CXX language must not be enabled."
    )
endif()

set(_catime_forbidden_extensions
    .inc
    .cc
    .cpp
    .cxx
    .c++
    .hpp
    .hh
    .hxx
    .inl
    .ipp
    .tpp
    .ixx
    .cppm
    .m
    .mm
    .cu
    .cuh
)

set(_catime_forbidden_files)
foreach(_catime_root IN ITEMS src include tests)
    file(GLOB_RECURSE _catime_first_party_files
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_catime_root}/*"
    )
    foreach(_catime_file IN LISTS _catime_first_party_files)
        get_filename_component(_catime_name "${_catime_file}" NAME)
        string(REGEX MATCH "\\.[^.]+$" _catime_extension "${_catime_name}")
        string(TOLOWER "${_catime_extension}" _catime_extension)
        if(_catime_extension IN_LIST _catime_forbidden_extensions)
            list(APPEND _catime_forbidden_files "${_catime_file}")
        endif()
    endforeach()
endforeach()

if(_catime_forbidden_files)
    list(REMOVE_DUPLICATES _catime_forbidden_files)
    list(SORT _catime_forbidden_files)
    set(_catime_forbidden_display)
    foreach(_catime_file IN LISTS _catime_forbidden_files)
        file(RELATIVE_PATH _catime_relative
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${_catime_file}"
        )
        string(REPLACE "\\" "/" _catime_relative "${_catime_relative}")
        list(APPEND _catime_forbidden_display "${_catime_relative}")
    endforeach()
    string(JOIN "\n  " _catime_forbidden_text
        ${_catime_forbidden_display}
    )
    message(FATAL_ERROR
        "Catime is a C-only project. Remove forbidden source files:\n"
        "  ${_catime_forbidden_text}"
    )
endif()

unset(_catime_enabled_languages)
unset(_catime_forbidden_extensions)
unset(_catime_forbidden_files)
unset(_catime_forbidden_display)
unset(_catime_forbidden_text)
unset(_catime_first_party_files)
unset(_catime_root)
unset(_catime_file)
unset(_catime_name)
unset(_catime_extension)
unset(_catime_relative)
