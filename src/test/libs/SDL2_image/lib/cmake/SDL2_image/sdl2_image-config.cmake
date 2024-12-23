# sdl2_image cmake project-config input for ./configure scripts

include(FeatureSummary)
set_package_properties(SDL2_image PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_image/"
    DESCRIPTION "SDL_image is an image file loading library"
)

set(SDL2_image_FOUND TRUE)

set(SDL2IMAGE_AVIF  1)
set(SDL2IMAGE_BMP   1)
set(SDL2IMAGE_GIF   1)
set(SDL2IMAGE_JPG   1)
set(SDL2IMAGE_JXL   0)
set(SDL2IMAGE_LBM   1)
set(SDL2IMAGE_PCX   1)
set(SDL2IMAGE_PNG   1)
set(SDL2IMAGE_PNM   1)
set(SDL2IMAGE_QOI   1)
set(SDL2IMAGE_SVG   1)
set(SDL2IMAGE_TGA   1)
set(SDL2IMAGE_TIF   1)
set(SDL2IMAGE_XCF   1)
set(SDL2IMAGE_XPM   1)
set(SDL2IMAGE_XV    1)
set(SDL2IMAGE_WEBP  1)

set(SDL2IMAGE_JPG_SAVE 1)
set(SDL2IMAGE_PNG_SAVE 1)

set(SDL2IMAGE_VENDORED  FALSE)

set(SDL2IMAGE_BACKEND_IMAGEIO   0)
set(SDL2IMAGE_BACKEND_STB       1)
set(SDL2IMAGE_BACKEND_WIC       0)

get_filename_component(CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_DIR} REALPATH)
get_filename_component(prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(exec_prefix "${prefix}")
set(bindir "${prefix}/bin")
set(includedir "${prefix}/include")
set(libdir "${prefix}/lib")
set(_sdl2image_extra_static_libraries " ")
string(STRIP "${_sdl2image_extra_static_libraries}" _sdl2image_extra_static_libraries)

set(_sdl2image_bindir   "${bindir}")
set(_sdl2image_libdir   "${libdir}")
set(_sdl2image_incdir   "${includedir}/SDL2")

# Convert _sdl2image_extra_static_libraries to list and keep only libraries
string(REGEX MATCHALL "(-[lm]([-a-zA-Z0-9._]+))|(-Wl,[^ ]*framework[^ ]*)" _sdl2image_extra_static_libraries "${_sdl2image_extra_static_libraries}")
string(REGEX REPLACE "^-l" "" _sdl2image_extra_static_libraries "${_sdl2image_extra_static_libraries}")
string(REGEX REPLACE ";-l" ";" _sdl2image_extra_static_libraries "${_sdl2image_extra_static_libraries}")

unset(prefix)
unset(exec_prefix)
unset(bindir)
unset(includedir)
unset(libdir)

include(CMakeFindDependencyMacro)

if(NOT TARGET SDL2_image::SDL2_image)
    add_library(SDL2_image::SDL2_image SHARED IMPORTED)
    set_target_properties(SDL2_image::SDL2_image
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl2image_incdir}"
            COMPATIBLE_INTERFACE_BOOL "SDL2_SHARED"
            INTERFACE_SDL2_SHARED "ON"
    )
    if(WIN32)
        set_target_properties(SDL2_image::SDL2_image
            PROPERTIES
                IMPORTED_LOCATION "${_sdl2image_bindir}/SDL2_image.dll"
                IMPORTED_IMPLIB "${_sdl2image_libdir}/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2_image.dll${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    else()
        set_target_properties(SDL2_image::SDL2_image
            PROPERTIES
                IMPORTED_LOCATION "${_sdl2image_libdir}/${CMAKE_SHARED_LIBRARY_PREFIX}SDL2_image${CMAKE_SHARED_LIBRARY_SUFFIX}"
        )
    endif()
endif()

if(NOT TARGET SDL2_image::SDL2_image-static)
    add_library(SDL2_image::SDL2_image-static STATIC IMPORTED)

    set_target_properties(SDL2_image::SDL2_image-static
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl2image_incdir}"
            IMPORTED_LOCATION "${_sdl2image_libdir}/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2_image${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_LINK_LIBRARIES "${_sdl2image_extra_static_libraries}"
    )
endif()

unset(_sdl2image_extra_static_libraries)
unset(_sdl2image_bindir)
unset(_sdl2image_libdir)
unset(_sdl2image_incdir)
