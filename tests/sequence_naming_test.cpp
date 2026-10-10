// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/sequence_naming.h"

using namespace imagemgr;

namespace {

std::string next_name(std::vector<std::string> names, std::string_view extension) {
    return next_sequential_filename(std::span<const std::string>(names), extension);
}

} // namespace

TEST_CASE("sequence naming: starts at 1 when the directory is empty", "[sequence-naming]") {
    REQUIRE(next_name({}, "png") == "1.png");
}

TEST_CASE("sequence naming: increments the largest digit stem", "[sequence-naming]") {
    REQUIRE(next_name({"1.png", "2.png"}, "png") == "3.png");
    REQUIRE(next_name({"a.png", "007.png"}, "png") == "008.png");
}

TEST_CASE("sequence naming: zero-pads to the width of the maximum stem",
          "[sequence-naming]") {
    REQUIRE(next_name({"009.png", "010.png"}, "png") == "011.png");
    REQUIRE(next_name({"1.png", "099.png"}, "png") == "100.png");
}

TEST_CASE("sequence naming: ignores non-numeric stems", "[sequence-naming]") {
    REQUIRE(next_name({"photo.png", "holiday.jpg"}, "png") == "1.png");
}

TEST_CASE("sequence naming: the sequence is shared across extensions", "[sequence-naming]") {
    REQUIRE(next_name({"1.jpg", "2.jpg"}, "png") == "3.png");
}

TEST_CASE("sequence naming: a file without an extension still counts", "[sequence-naming]") {
    REQUIRE(next_name({"5", "6"}, "png") == "7.png");
}
