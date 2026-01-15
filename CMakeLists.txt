project(flood-tuber VERSION 0.1.0)


set(flood-tuber_SOURCES
    flood-tuber.cpp
    flood-tuber.h
    webp-decoder.cpp
    webp-decoder.h
)


add_library(flood-tuber MODULE
    ${flood-tuber_SOURCES}
)


include(FetchContent)

FetchContent_Declare(
    libwebp
    GIT_REPOSITORY https://github.com/webmproject/libwebp.git
    GIT_TAG        v1.3.2
)
# Disable libwebp tools and extras to speed up build and avoid dependencies
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libwebp)

target_link_libraries(flood-tuber PRIVATE
    libobs
    webp
    webpdemux
)


set_target_properties(flood-tuber PROPERTIES
    FOLDER "plugins"
    PREFIX ""
)


add_custom_command(TARGET flood-tuber POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:flood-tuber>"
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit/$<TARGET_FILE_NAME:flood-tuber>"
    VERBATIM
)


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_custom_command(TARGET flood-tuber POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/data/obs-plugins/flood-tuber"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/data/obs-plugins/flood-tuber"
        VERBATIM
    )
endif()