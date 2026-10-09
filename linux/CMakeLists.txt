cmake_minimum_required(VERSION 3.16)
project(catime-linux C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(CATIME_LINUX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Locate the shared Catime resources (source tree, one level up).
get_filename_component(CATIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CATIME_RESOURCE_DIR "${CATIME_ROOT}/resource")
set(CATIME_LIBS_DIR "${CATIME_ROOT}/libs")

if(NOT EXISTS "${CATIME_RESOURCE_DIR}/languages/en.ini")
    message(WARNING
        "Catime resource tree not found at ${CATIME_RESOURCE_DIR}. "
        "Language files will be missing at runtime.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(X11 REQUIRED x11)

set(CATIME_SOURCES
    src/main.c
    src/app.c
    src/config.c
    src/timer.c
    src/i18n.c
    src/paths.c
    src/log.c
    src/render.c
    src/window.c
    src/tray.c
    src/notify.c
    src/audio.c
    src/hotkey.c
    src/single.c
    ${CATIME_LIBS_DIR}/miniaudio/miniaudio.h
)

add_executable(catime ${CATIME_SOURCES})

target_include_directories(catime PRIVATE
    src
    ${CATIME_LIBS_DIR}/miniaudio
)

target_compile_options(catime PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
    -Wno-unused-variable -Wno-sign-compare
    ${GTK3_CFLAGS} ${APPINDICATOR_CFLAGS} ${LIBNOTIFY_CFLAGS} ${X11_CFLAGS}
)

if(CATIME_LINUX_WARNINGS_AS_ERRORS)
    target_compile_options(catime PRIVATE -Werror)
endif()

target_link_libraries(catime PRIVATE
    ${GTK3_LIBRARIES}
    ${APPINDICATOR_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${X11_LIBRARIES}
    m pthread
)

target_link_directories(catime PRIVATE
    ${GTK3_LIBRARY_DIRS}
    ${APPINDICATOR_LIBRARY_DIRS}
    ${LIBNOTIFY_LIBRARY_DIRS}
    ${X11_LIBRARY_DIRS}
)

# Install: binary + language data so it runs from anywhere.
install(TARGETS catime RUNTIME DESTINATION bin)
install(DIRECTORY ${CATIME_RESOURCE_DIR}/languages
        DESTINATION share/catime)
install(FILES data/catime.desktop DESTINATION share/applications)
install(FILES data/icons/catime.png DESTINATION share/icons/hicolor/256x256/apps)
