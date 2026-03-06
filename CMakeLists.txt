cmake_minimum_required(VERSION 3.22...3.25)
project(flood-tuber VERSION 1.0.2)

# 1. Plugin Definition
add_library(flood-tuber MODULE)
set_target_properties_obs(flood-tuber PROPERTIES FOLDER plugins PREFIX "")

# 2. Dependencies (libwebp)
include(FetchContent)
FetchContent_Declare(
    libwebp
    GIT_REPOSITORY https://github.com/webmproject/libwebp.git
    GIT_TAG        v1.3.2
)
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

# 3. Source Files
target_sources(flood-tuber PRIVATE
    flood-tuber.cpp
    flood-tuber.h
    flood-tuber-props.cpp
    flood-tuber-props.h
    flood-tuber-lipsync.cpp
    flood-tuber-lipsync.h
    webp-decoder.cpp
    webp-decoder.h
    lodepng.cpp
    lodepng.h
    apng-decoder.cpp
    apng-decoder.h
)

# 4. Libraries (Link OBS::libobs + OVR LipSync Shim)
target_include_directories(flood-tuber PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ovr-test/Include
)
target_link_libraries(flood-tuber PRIVATE
    OBS::libobs
    webp
    webpdemux
    ${CMAKE_CURRENT_SOURCE_DIR}/ovr-test/Lib/Win64/OVRLipSyncShim.lib
)

# 5. Data directory copy
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
    foreach(data_file ${data_files})
        file(RELATIVE_PATH dest_file "${CMAKE_CURRENT_SOURCE_DIR}/data" "${data_file}")
        get_filename_component(dest_dir "${dest_file}" DIRECTORY)
        add_custom_command(
            TARGET flood-tuber POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/data/obs-plugins/flood-tuber/${dest_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${data_file}" "${CMAKE_BINARY_DIR}/data/obs-plugins/flood-tuber/${dest_file}"
            VERBATIM
        )
    endforeach()
endif()