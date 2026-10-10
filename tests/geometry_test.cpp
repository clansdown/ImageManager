// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include "core/geometry.h"

using namespace imagemgr;

TEST_CASE("geometry: a rectangle already inside the bounds is unchanged", "[geometry]") {
    const ImageRectangle rectangle{10, 20, 30, 40};

    REQUIRE(clamp_rectangle(rectangle, ImageSize{100, 100}) == rectangle);
}

TEST_CASE("geometry: a rectangle is shifted to stay inside the bounds", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{90, 95, 30, 40}, ImageSize{100, 100}) ==
            ImageRectangle{70, 60, 30, 40});
}

TEST_CASE("geometry: an oversized rectangle is pinned to the far edge", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{50, 50, 80, 80}, ImageSize{100, 100}) ==
            ImageRectangle{20, 20, 80, 80});
}

TEST_CASE("geometry: negative positions are clamped to the origin", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{-10, -5, 30, 40}, ImageSize{100, 100}) ==
            ImageRectangle{0, 0, 30, 40});
}

TEST_CASE("geometry: sizes larger than the bounds are truncated", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{0, 0, 200, 200}, ImageSize{100, 100}) ==
            ImageRectangle{0, 0, 100, 100});
}

TEST_CASE("geometry: a non-positive size normalises to an empty rectangle", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{10, 10, -5, -5}, ImageSize{100, 100}) ==
            ImageRectangle{});
    REQUIRE(clamp_rectangle(ImageRectangle{10, 10, 0, 40}, ImageSize{100, 100}) ==
            ImageRectangle{});
}

TEST_CASE("geometry: empty bounds produce an empty rectangle", "[geometry]") {
    REQUIRE(clamp_rectangle(ImageRectangle{0, 0, 10, 10}, ImageSize{0, 0}) == ImageRectangle{});
}
