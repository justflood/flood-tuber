cmake_minimum_required(VERSION 3.22...3.30)

if(COMMAND set_target_properties_obs)
    # In-tree build inside obs-studio/plugins/flood-tuber
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" _buildspec)
    string(JSON _version GET "${_buildspec}" version)
    project(flood-tuber VERSION ${_version})
    set(_target_name flood-tuber)
    add_library(${_target_name} MODULE)
    set_target_properties_obs(${_target_name} PROPERTIES FOLDER plugins PREFIX "")
else()
    # Standalone / CI build (obs-plugintemplate)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
    project(${_name} VERSION ${_version})

    option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
    option(ENABLE_QT "Use Qt functionality" OFF)

    include(compilerconfig)
    include(defaults)
    include(helpers)

    set(_target_name ${CMAKE_PROJECT_NAME})
    add_library(${_target_name} MODULE)
    find_package(libobs REQUIRED)
endif()

# Version passed to C++ code via compile define (single source of truth)
target_compile_definitions(${_target_name} PRIVATE
    FLOOD_TUBER_VERSION="${PROJECT_VERSION}"
)

target_compile_options(${_target_name} PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-variable -Wno-unused-parameter>
)

# Dependencies (libwebp)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

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

FetchContent_GetProperties(libwebp)
if(NOT libwebp_POPULATED)
    FetchContent_Populate(libwebp)
    add_subdirectory(${libwebp_SOURCE_DIR} ${libwebp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Source Files
target_sources(${_target_name} PRIVATE
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

# Libraries (Link OBS::libobs + libwebp)
target_link_libraries(${_target_name} PRIVATE
    OBS::libobs
    webp
    webpdemux
)

if(COMMAND set_target_properties_obs)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
        file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
        foreach(data_file ${data_files})
            file(RELATIVE_PATH dest_file "${CMAKE_CURRENT_SOURCE_DIR}/data" "${data_file}")
            get_filename_component(dest_dir "${dest_file}" DIRECTORY)
            add_custom_command(
                TARGET ${_target_name} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/data/obs-plugins/flood-tuber/${dest_dir}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${data_file}" "${CMAKE_BINARY_DIR}/data/obs-plugins/flood-tuber/${dest_file}"
                VERBATIM
            )
        endforeach()
    endif()
else()
    set_target_properties_plugin(${_target_name} PROPERTIES OUTPUT_NAME ${_name})
endif()
