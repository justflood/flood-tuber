cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

# FetchContent for libwebp
include(FetchContent)
FetchContent_Declare(
    libwebp
    GIT_REPOSITORY https://chromium.googlesource.com/webm/libwebp
    GIT_TAG        v1.3.2
)
# libwebp options
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "Build animation utilities" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "Build cwebp" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "Build dwebp" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "Build gif2webp" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "Build img2webp" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "Build vwebp" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "Build webpinfo" FORCE)
set(WEBP_BUILD_LIBWEBPMUX ON CACHE BOOL "Build libwebpmux" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "Build webpmux" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "Build extras" FORCE)
FetchContent_MakeAvailable(libwebp)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
    flood-tuber.cpp
    flood-tuber.h
    flood-tuber-props.cpp
    flood-tuber-props.h
    apng-decoder.cpp
    apng-decoder.h
    webp-decoder.cpp
    webp-decoder.h
    lodepng.cpp
    lodepng.h
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE webp webpdemux)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
