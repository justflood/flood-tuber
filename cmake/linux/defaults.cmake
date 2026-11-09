# CMake Linux defaults module

include_guard(GLOBAL)

# Set default installation directories
include(GNUInstallDirs)

if(CMAKE_INSTALL_LIBDIR MATCHES "(CMAKE_SYSTEM_PROCESSOR)")
  string(REPLACE "CMAKE_SYSTEM_PROCESSOR" "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_SET_DESTDIR ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
  set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
endif()

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
set(
  CPACK_SOURCE_IGNORE_FILES
  ".*~$"
  \\.git/
  \\.github/
  \\.gitignore
  \\.ccache/
  build_.*
  cmake/\\.CMakeBuildNumber
  release/
)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)

include(CPack)

find_package(libobs QUIET)

if(NOT TARGET OBS::libobs)
  find_package(LibObs QUIET)
  if(TARGET libobs)
    add_library(OBS::libobs ALIAS libobs)
  elseif(NOT TARGET OBS::libobs)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(LIBOBS QUIET libobs)
    endif()

    if(LIBOBS_FOUND)
      add_library(OBS::libobs INTERFACE IMPORTED)
      target_include_directories(OBS::libobs INTERFACE ${LIBOBS_INCLUDE_DIRS})
      target_link_libraries(OBS::libobs INTERFACE ${LIBOBS_LINK_LIBRARIES})
    else()
      message(STATUS "libobs not found in system paths; fetching headers from obs-studio...")
      if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
      endif()
      include(FetchContent)
      FetchContent_Declare(
        obs_headers
        GIT_REPOSITORY https://github.com/obsproject/obs-studio.git
        GIT_TAG        33.0.0
        GIT_SHALLOW    TRUE
      )
      FetchContent_GetProperties(obs_headers)
      if(NOT obs_headers_POPULATED)
        FetchContent_Populate(obs_headers)
      endif()

      FetchContent_Declare(
        simde
        GIT_REPOSITORY https://github.com/simd-everywhere/simde.git
        GIT_TAG        v0.8.2
        GIT_SHALLOW    TRUE
      )
      FetchContent_GetProperties(simde)
      if(NOT simde_POPULATED)
        FetchContent_Populate(simde)
      endif()

      if(NOT EXISTS "${obs_headers_BINARY_DIR}/obsconfig.h")
        file(WRITE "${obs_headers_BINARY_DIR}/obsconfig.h" "#pragma once\n")
      endif()

      add_library(OBS::libobs INTERFACE IMPORTED)
      target_include_directories(OBS::libobs INTERFACE 
        ${obs_headers_SOURCE_DIR}/libobs
        ${obs_headers_SOURCE_DIR}/libobs/util
        ${obs_headers_SOURCE_DIR}/libobs/graphics
        ${obs_headers_BINARY_DIR}
        ${simde_SOURCE_DIR}
      )
    endif()
  endif()

  if(ENABLE_FRONTEND_API)
    find_path(
      obs-frontend-api_INCLUDE_DIR
      NAMES obs-frontend-api.h
      PATHS /usr/include /usr/local/include
      PATH_SUFFIXES obs
    )

    find_library(obs-frontend-api_LIBRARY NAMES obs-frontend-api PATHS /usr/lib /usr/local/lib)

    if(obs-frontend-api_LIBRARY)
      if(NOT TARGET OBS::obs-frontend-api)
        if(IS_ABSOLUTE "${obs-frontend-api_LIBRARY}")
          add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LOCATION "${obs-frontend-api_LIBRARY}")
        else()
          add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
          set_property(TARGET OBS::obs-frontend-api PROPERTY IMPORTED_LIBNAME "${obs-frontend-api_LIBRARY}")
        endif()

        set_target_properties(
          OBS::obs-frontend-api
          PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${obs-frontend-api_INCLUDE_DIR}"
        )
      endif()
    endif()
  endif()

  macro(find_package)
    if(NOT "${ARGV0}" STREQUAL libobs AND NOT "${ARGV0}" STREQUAL obs-frontend-api)
      _find_package(${ARGV})
    endif()
  endmacro()
endif()
