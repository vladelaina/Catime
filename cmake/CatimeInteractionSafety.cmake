# Keep Catime's desktop interaction compatible with game anti-cheat systems.
# These APIs are unnecessary for the application and are easy to mistake for
# global input interception or cross-process injection behavior.
set(_catime_forbidden_interaction_apis
    SetWindowsHookEx
    SetWindowsHookExA
    SetWindowsHookExW
    CallNextHookEx
    UnhookWindowsHookEx
    SendInput
    mouse_event
    keybd_event
    GetAsyncKeyState
    RegisterRawInputDevices
    GetRawInputData
    SetWinEventHook
    CreateRemoteThread
    CreateRemoteThreadEx
    WriteProcessMemory
    ReadProcessMemory
    VirtualAllocEx
    VirtualProtectEx
    DebugActiveProcess
    AllowSetForegroundWindow
)

if(CMAKE_SCRIPT_MODE_FILE)
    get_filename_component(_catime_interaction_source_root
        "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    file(GLOB_RECURSE _catime_interaction_source_files
        LIST_DIRECTORIES FALSE
        "${_catime_interaction_source_root}/src/*.c"
        "${_catime_interaction_source_root}/src/*.h"
        "${_catime_interaction_source_root}/include/*.h"
    )
else()
    set(_catime_interaction_source_root "${CMAKE_CURRENT_SOURCE_DIR}")
    file(GLOB_RECURSE _catime_interaction_source_files
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${_catime_interaction_source_root}/src/*.c"
        "${_catime_interaction_source_root}/src/*.h"
        "${_catime_interaction_source_root}/include/*.h"
    )
endif()

set(_catime_interaction_violations)
foreach(_catime_file IN LISTS _catime_interaction_source_files)
    file(READ "${_catime_file}" _catime_contents)
    file(RELATIVE_PATH _catime_relative
        "${_catime_interaction_source_root}"
        "${_catime_file}"
    )
    string(REPLACE "\\" "/" _catime_relative "${_catime_relative}")

    foreach(_catime_api IN LISTS _catime_forbidden_interaction_apis)
        if(_catime_contents MATCHES
           "(^|[^A-Za-z0-9_])${_catime_api}([^A-Za-z0-9_]|$)")
            list(APPEND _catime_interaction_violations
                "${_catime_relative}: ${_catime_api}"
            )
        endif()
    endforeach()

    if(_catime_contents MATCHES
       "(^|[^A-Za-z0-9_])AttachThreadInput([^A-Za-z0-9_]|$)" AND
       NOT _catime_relative STREQUAL "src/tray/tray_menu_tracking.c")
        list(APPEND _catime_interaction_violations
            "${_catime_relative}: AttachThreadInput outside the Shell-only tray path"
        )
    endif()
endforeach()

if(_catime_interaction_violations)
    list(REMOVE_DUPLICATES _catime_interaction_violations)
    list(SORT _catime_interaction_violations)
    string(JOIN "\n  " _catime_interaction_violation_text
        ${_catime_interaction_violations}
    )
    message(FATAL_ERROR
        "Unsafe global-input or cross-process interaction APIs detected:\n"
        "  ${_catime_interaction_violation_text}"
    )
endif()

if(NOT CMAKE_SCRIPT_MODE_FILE AND
   NOT TARGET catime_interaction_safety_check)
    add_custom_target(catime_interaction_safety_check
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_FILE}"
        COMMENT "Checking desktop interaction safety"
        VERBATIM
    )
endif()

unset(_catime_forbidden_interaction_apis)
unset(_catime_interaction_source_root)
unset(_catime_interaction_source_files)
unset(_catime_interaction_violations)
unset(_catime_interaction_violation_text)
unset(_catime_contents)
unset(_catime_relative)
unset(_catime_file)
unset(_catime_api)
