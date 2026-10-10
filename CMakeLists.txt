# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.24)

project(ImageManager
    VERSION 0.1.0
    DESCRIPTION "Browse images in a directory and extract sub-regions into new images"
    HOMEPAGE_URL "https://example.invalid/ImageManager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
# Resolve gtkmm and all of its transitive dependencies (glibmm, giomm, sigc++,
# GTK, ...) from pkg-config.
pkg_check_modules(GTKMM4 REQUIRED gtkmm-4.0)

# Codec libraries used directly to write WebP and AVIF: gdk-pixbuf can read
# them but has no savers for either.
pkg_check_modules(LIBWEBP REQUIRED libwebp)
pkg_check_modules(LIBHEIF REQUIRED libheif)

# Baseline warning set. Applied only to first-party targets (never inherited by
# fetched third-party code such as Catch2).
add_library(imagemgr_warnings INTERFACE)
target_compile_options(imagemgr_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)

# gtkmm and its transitive dependencies. The dependency headers are marked
# SYSTEM so the warning set above applies to our code only.
add_library(imagemgr_gtk INTERFACE)
target_include_directories(imagemgr_gtk SYSTEM INTERFACE ${GTKMM4_INCLUDE_DIRS})
target_compile_options(imagemgr_gtk INTERFACE ${GTKMM4_CFLAGS_OTHER})
target_link_directories(imagemgr_gtk INTERFACE ${GTKMM4_LIBRARY_DIRS})
target_link_options(imagemgr_gtk INTERFACE ${GTKMM4_LDFLAGS_OTHER})
target_link_libraries(imagemgr_gtk INTERFACE ${GTKMM4_LIBRARIES})

# Headless core. Header-only for now; exposes the src/ include root so targets
# can include "core/...", "io/..." and "app/...".
add_library(imagemgr_core INTERFACE)
target_include_directories(imagemgr_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# io: directory scanning, image loading/saving, codecs and preference storage.
add_library(imagemgr_io STATIC
    src/io/command_line_options.cpp
    src/io/directory_scanner.cpp
    src/io/export_image.cpp
    src/io/image_document.cpp
    src/io/image_encoder.cpp
    src/io/image_load_service.cpp
    src/io/image_loader.cpp
    src/io/preferences_store.cpp
    src/io/thumbnail_cache.cpp
    src/io/thumbnail_service.cpp)
target_link_libraries(imagemgr_io PUBLIC imagemgr_core imagemgr_gtk)
target_link_libraries(imagemgr_io PRIVATE
    imagemgr_warnings
    ${LIBWEBP_LIBRARIES}
    ${LIBHEIF_LIBRARIES})
target_include_directories(imagemgr_io SYSTEM PRIVATE
    ${LIBWEBP_INCLUDE_DIRS}
    ${LIBHEIF_INCLUDE_DIRS})

# gtkmm application widgets and wiring.
add_library(imagemgr_app STATIC
    src/app/application.cpp
    src/app/directory_item.cpp
    src/app/directory_pane.cpp
    src/app/export_controller.cpp
    src/app/image_canvas.cpp
    src/app/image_pane.cpp
    src/app/main_window.cpp
    src/app/settings_dialog.cpp
    src/app/thumbnail_cell.cpp
    src/app/toast.cpp)
target_link_libraries(imagemgr_app PUBLIC imagemgr_io)
target_link_libraries(imagemgr_app PRIVATE imagemgr_warnings)

add_executable(imagemgr src/main.cpp)
target_link_libraries(imagemgr PRIVATE imagemgr_app imagemgr_warnings)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
