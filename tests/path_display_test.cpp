// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/path_display.h"

using namespace imagemgr;

TEST_CASE("path display: the home directory itself becomes ~", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/home/user", "/home/user") == "~");
}

TEST_CASE("path display: a path under home becomes ~/...", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/home/user/projects/ImageManager", "/home/user") ==
            "~/projects/ImageManager");
}

TEST_CASE("path display: a trailing separator on home is ignored", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/home/user/pictures", "/home/user/") == "~/pictures");
    REQUIRE(abbreviate_home_directory("/home/user", "/home/user/") == "~");
}

TEST_CASE("path display: a sibling of home is not abbreviated", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/home/userx", "/home/user") == "/home/userx");
    REQUIRE(abbreviate_home_directory("/home/userx/pictures", "/home/user") ==
            "/home/userx/pictures");
}

TEST_CASE("path display: a path outside home is unchanged", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/srv/images", "/home/user") == "/srv/images");
}

TEST_CASE("path display: an empty or root home leaves the path unchanged", "[path-display]") {
    REQUIRE(abbreviate_home_directory("/srv/images", "") == "/srv/images");
    REQUIRE(abbreviate_home_directory("/srv/images", "/") == "/srv/images");
}
