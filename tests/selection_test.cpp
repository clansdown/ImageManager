// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include "core/selection.h"

using namespace imagemgr;

namespace {

constexpr double kRadius = 5.0;
constexpr ImageSize kBounds{100, 100};
// left=10, top=10, right=50, bottom=40, centre=(30, 25).
constexpr ImageRectangle kRegion{10, 10, 40, 30};

} // namespace

TEST_CASE("selection: hit-test classifies the interior and the exterior", "[selection]") {
    REQUIRE(hit_test_selection(kRegion, PixelPoint{30.0, 25.0}, kRadius) ==
            SelectionHandle::Inside);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{95.0, 95.0}, kRadius) ==
            SelectionHandle::None);
}

TEST_CASE("selection: hit-test finds the corners", "[selection]") {
    REQUIRE(hit_test_selection(kRegion, PixelPoint{11.0, 11.0}, kRadius) ==
            SelectionHandle::TopLeft);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{49.0, 11.0}, kRadius) ==
            SelectionHandle::TopRight);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{49.0, 39.0}, kRadius) ==
            SelectionHandle::BottomRight);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{11.0, 39.0}, kRadius) ==
            SelectionHandle::BottomLeft);
}

TEST_CASE("selection: hit-test finds the edges", "[selection]") {
    REQUIRE(hit_test_selection(kRegion, PixelPoint{30.0, 10.0}, kRadius) ==
            SelectionHandle::Top);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{50.0, 25.0}, kRadius) ==
            SelectionHandle::Right);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{30.0, 40.0}, kRadius) ==
            SelectionHandle::Bottom);
    REQUIRE(hit_test_selection(kRegion, PixelPoint{10.0, 25.0}, kRadius) ==
            SelectionHandle::Left);
}

TEST_CASE("selection: a corner wins over an adjacent edge", "[selection]") {
    // On the top edge but within the corner grab radius.
    REQUIRE(hit_test_selection(kRegion, PixelPoint{12.0, 10.0}, kRadius) ==
            SelectionHandle::TopLeft);
}

TEST_CASE("selection: dragging a corner resizes the rectangle", "[selection]") {
    REQUIRE(resize_selection(kRegion, SelectionHandle::BottomRight, PixelPoint{70.0, 60.0},
                             kBounds) == ImageRectangle{10, 10, 60, 50});
    REQUIRE(resize_selection(kRegion, SelectionHandle::TopLeft, PixelPoint{5.0, 5.0},
                             kBounds) == ImageRectangle{5, 5, 45, 35});
}

TEST_CASE("selection: dragging an edge moves just that edge", "[selection]") {
    REQUIRE(resize_selection(kRegion, SelectionHandle::Right, PixelPoint{70.0, 25.0},
                             kBounds) == ImageRectangle{10, 10, 60, 30});
    REQUIRE(resize_selection(kRegion, SelectionHandle::Top, PixelPoint{30.0, 4.0}, kBounds) ==
            ImageRectangle{10, 4, 40, 36});
}

TEST_CASE("selection: dragging past the opposite edge flips the rectangle", "[selection]") {
    REQUIRE(resize_selection(kRegion, SelectionHandle::Left, PixelPoint{80.0, 25.0}, kBounds) ==
            ImageRectangle{50, 10, 30, 30});
}

TEST_CASE("selection: resizing is clamped to the bounds", "[selection]") {
    REQUIRE(resize_selection(kRegion, SelectionHandle::BottomRight, PixelPoint{200.0, 200.0},
                             kBounds) == ImageRectangle{10, 10, 90, 90});
}

TEST_CASE("selection: resizing with a non-edge handle is a no-op", "[selection]") {
    REQUIRE(resize_selection(kRegion, SelectionHandle::Inside, PixelPoint{80.0, 80.0}, kBounds) ==
            kRegion);
    REQUIRE(resize_selection(kRegion, SelectionHandle::None, PixelPoint{80.0, 80.0}, kBounds) ==
            kRegion);
}

TEST_CASE("selection: moving translates the rectangle", "[selection]") {
    REQUIRE(move_selection(kRegion, PixelPoint{5.0, 7.0}, kBounds) ==
            ImageRectangle{15, 17, 40, 30});
}

TEST_CASE("selection: moving is clamped to the bounds", "[selection]") {
    REQUIRE(move_selection(kRegion, PixelPoint{-100.0, -100.0}, kBounds) ==
            ImageRectangle{0, 0, 40, 30});
    REQUIRE(move_selection(kRegion, PixelPoint{100.0, 100.0}, kBounds) ==
            ImageRectangle{60, 70, 40, 30});
}
