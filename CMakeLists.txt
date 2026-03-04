cmake_minimum_required(VERSION 3.22...3.25)
project(flood-tuber VERSION 1.0.0)

# 1. OBS 30 Modern Plugin Tanımlaması
obs_add_plugin(flood-tuber)

# 2. Bağımlılıklar (libwebp) - Senin kodunla aynı, sadece yeri düzenlendi
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

# 3. Kaynak Dosyaların Eklenmesi
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

# 4. Kütüphanelerin Bağlanması (DİKKAT: libobs yerine OBS::libobs kullanıyoruz)
target_link_libraries(flood-tuber PRIVATE
    OBS::libobs
    webp
    webpdemux
)

# 5. Data Klasörünün Otomatik Kopyalanması (O uzun custom_command ameleliğinin bittiği yer)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    obs_add_plugin_data(flood-tuber "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()