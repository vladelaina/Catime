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
    set(CATIME_SIZE_OPTIMIZATION_FLAG -Os)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-Oz" CATIME_COMPILER_SUPPORTS_OZ)
        if(CATIME_COMPILER_SUPPORTS_OZ)
            set(CATIME_SIZE_OPTIMIZATION_FLAG -Oz)
        endif()
    endif()
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
    target_link_options(catime PRIVATE
        -mwindows
        -flto
        -Wl,--gc-sections
        -Wl,--strip-all
        -Wl,--build-id=none
        -Wl,--no-insert-timestamp
        -static
    )
else()
    target_compile_options(catime PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-g>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unknown-warning-option>
    )
    target_link_options(catime PRIVATE -mwindows -static)
endif()
