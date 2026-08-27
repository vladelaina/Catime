if(MSVC)
    target_compile_definitions(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:_CRT_SECURE_NO_WARNINGS>
        $<$<COMPILE_LANGUAGE:C>:_CRT_NONSTDC_NO_WARNINGS>
        $<$<COMPILE_LANGUAGE:C>:strcasecmp=_stricmp>
        $<$<COMPILE_LANGUAGE:C>:strdup=_strdup>
    )
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:/std:c11>
        $<$<COMPILE_LANGUAGE:C>:/utf-8>
    )
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
            $<$<COMPILE_LANGUAGE:C>:/MP4>
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
        target_link_options(catime PRIVATE /DEBUG /INCREMENTAL:NO)
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
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${CATIME_RELEASE_OPTIMIZATION}>
        $<$<COMPILE_LANGUAGE:C>:-mtune=generic>
        $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:C>:-fno-strict-aliasing>
        $<$<COMPILE_LANGUAGE:C>:-flto>
        $<$<COMPILE_LANGUAGE:C>:-fno-omit-frame-pointer>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unknown-warning-option>
    )
    # Win32 stack walking can use the retained frame pointers; Catime does not
    # use GCC exception unwinding, so per-function DWARF tables are redundant.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
       CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fno-unwind-tables>
            $<$<COMPILE_LANGUAGE:C>:-fno-asynchronous-unwind-tables>
        )
    endif()
    if(CATIME_ENABLE_BINARY_HARDENING)
        target_compile_options(catime PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-fstack-protector-strong>
        )
        target_link_options(catime PRIVATE
            -Wl,--dynamicbase
            -Wl,--nxcompat
        )
    endif()
    target_link_options(catime PRIVATE
        -mwindows
        -flto
        -Wl,--gc-sections
        -static
    )
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_STRIP)
        add_custom_command(TARGET catime POST_BUILD
            COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:catime>"
            COMMENT "Removing symbols not needed at runtime"
            VERBATIM
        )
    endif()
else()
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-g>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unknown-warning-option>
    )
    target_link_options(catime PRIVATE -mwindows -static)
endif()
