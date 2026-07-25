cmake_minimum_required(VERSION 3.22...3.25)
project(flood-tuber VERSION 1.1.0)

find_package(libobs CONFIG REQUIRED)

# Platform-agnostic install destinations (fallback if OBS cmake helpers unavailable)
if(NOT DEFINED OBS_PLUGIN_DESTINATION)
    if(WIN32)
        set(OBS_PLUGIN_DESTINATION "obs-plugins/64bit")
    elseif(APPLE)
        set(OBS_PLUGIN_DESTINATION "obs-plugins")
    else()
        set(OBS_PLUGIN_DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    endif()
endif()
if(NOT DEFINED OBS_DATA_DESTINATION)
    if(WIN32)
        set(OBS_DATA_DESTINATION "data/obs-plugins")
    elseif(APPLE)
        set(OBS_DATA_DESTINATION "data/obs-plugins")
    else()
        set(OBS_DATA_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins")
    endif()
endif()

# 1. Plugin Definition
add_library(flood-tuber MODULE)
if(COMMAND set_target_properties_obs)
    set_target_properties_obs(flood-tuber PROPERTIES FOLDER plugins PREFIX "")
else()
    set_target_properties(flood-tuber PROPERTIES
        PREFIX ""
        FOLDER plugins
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obs-plugins"
    )
endif()

# Version passed to C++ code via compile define (single source of truth)
target_compile_definitions(flood-tuber PRIVATE
    FLOOD_TUBER_VERSION="${PROJECT_VERSION}"
)

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
    webp-decoder.cpp
    webp-decoder.h
    lodepng.cpp
    lodepng.h
    apng-decoder.cpp
    apng-decoder.h
)

# 4. Libraries (Link OBS::libobs)
target_link_libraries(flood-tuber PRIVATE
    OBS::libobs
    webp
    webpdemux
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

# 6. Install rules
install(TARGETS flood-tuber
    LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}"
    RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    DESTINATION "${OBS_DATA_DESTINATION}/flood-tuber"
    USE_SOURCE_PERMISSIONS)