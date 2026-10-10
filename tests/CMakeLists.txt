# SPDX-License-Identifier: GPL-3.0-only

# Prefer a system/vcpkg Catch2 (AGENTS.md: find_package(Catch2 3)). Fall back to
# fetching it so a fresh checkout still builds when the package is absent.
find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found; fetching it with FetchContent")
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(imagemgr_tests
    main.cpp
    smoke_test.cpp
    preferences_store_test.cpp
    command_line_options_test.cpp
    directory_scanner_test.cpp
    image_loader_test.cpp
    geometry_test.cpp
    rotation_test.cpp
    image_document_test.cpp
    selection_test.cpp
    sequence_naming_test.cpp
    sort_order_test.cpp
    image_encoder_test.cpp
    export_image_test.cpp
    thumbnail_cache_test.cpp)
target_link_libraries(imagemgr_tests PRIVATE
    Catch2::Catch2
    imagemgr_io
    imagemgr_warnings)

# Register the tests with CTest. When Catch2's extras are available, register
# each TEST_CASE individually; otherwise fall back to one entry for the binary.
# Either way a single test can be run directly:
#   ./build/tests/imagemgr_tests "<name-or-tag>"
include(Catch OPTIONAL)
if(COMMAND catch_discover_tests)
    catch_discover_tests(imagemgr_tests)
else()
    add_test(NAME imagemgr_tests COMMAND imagemgr_tests)
endif()
