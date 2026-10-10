// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include "core/orientation.h"

using namespace imagemgr;

namespace {

constexpr ExifOrientation kAll[] = {
    ExifOrientation::Normal,         ExifOrientation::FlipHorizontal,
    ExifOrientation::Rotate180,      ExifOrientation::FlipVertical,
    ExifOrientation::Transpose,      ExifOrientation::Rotate90,
    ExifOrientation::Transverse,     ExifOrientation::Rotate270,
};

} // namespace

TEST_CASE("orientation: clockwise composition table", "[orientation]") {
    REQUIRE(orientation_after_clockwise(ExifOrientation::Normal) == ExifOrientation::Rotate90);
    REQUIRE(orientation_after_clockwise(ExifOrientation::Rotate90) == ExifOrientation::Rotate180);
    REQUIRE(orientation_after_clockwise(ExifOrientation::Rotate180) ==
            ExifOrientation::Rotate270);
    REQUIRE(orientation_after_clockwise(ExifOrientation::Rotate270) == ExifOrientation::Normal);
    REQUIRE(orientation_after_clockwise(ExifOrientation::FlipHorizontal) ==
            ExifOrientation::Transpose);
    REQUIRE(orientation_after_clockwise(ExifOrientation::Transpose) ==
            ExifOrientation::FlipVertical);
    REQUIRE(orientation_after_clockwise(ExifOrientation::FlipVertical) ==
            ExifOrientation::Transverse);
    REQUIRE(orientation_after_clockwise(ExifOrientation::Transverse) ==
            ExifOrientation::FlipHorizontal);
}

TEST_CASE("orientation: counter-clockwise is the inverse of clockwise", "[orientation]") {
    for (const ExifOrientation orientation : kAll) {
        REQUIRE(orientation_after_counter_clockwise(orientation_after_clockwise(orientation)) ==
                orientation);
        REQUIRE(orientation_after_clockwise(orientation_after_counter_clockwise(orientation)) ==
                orientation);
    }
}

TEST_CASE("orientation: four clockwise turns return to the start", "[orientation]") {
    for (const ExifOrientation orientation : kAll) {
        ExifOrientation result = orientation;
        for (int turn = 0; turn < 4; ++turn) {
            result = orientation_after_clockwise(result);
        }
        REQUIRE(result == orientation);
    }
}
