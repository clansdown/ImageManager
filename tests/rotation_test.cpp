// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include "core/rotation.h"

using namespace imagemgr;

TEST_CASE("rotation: rotated_size swaps dimensions for quarter turns", "[rotation]") {
    const ImageSize size{640, 480};

    REQUIRE(rotated_size(size, QuarterTurns::None) == size);
    REQUIRE(rotated_size(size, QuarterTurns::Half) == size);
    REQUIRE(rotated_size(size, QuarterTurns::Clockwise) == ImageSize{480, 640});
    REQUIRE(rotated_size(size, QuarterTurns::CounterClockwise) == ImageSize{480, 640});
}

TEST_CASE("rotation: the quarter-turn state cycles in both directions", "[rotation]") {
    REQUIRE(turned_clockwise(QuarterTurns::None) == QuarterTurns::Clockwise);
    REQUIRE(turned_clockwise(QuarterTurns::Clockwise) == QuarterTurns::Half);
    REQUIRE(turned_clockwise(QuarterTurns::Half) == QuarterTurns::CounterClockwise);
    REQUIRE(turned_clockwise(QuarterTurns::CounterClockwise) == QuarterTurns::None);

    REQUIRE(turned_counter_clockwise(QuarterTurns::None) == QuarterTurns::CounterClockwise);
    REQUIRE(turned_counter_clockwise(QuarterTurns::CounterClockwise) == QuarterTurns::Half);
    REQUIRE(turned_counter_clockwise(QuarterTurns::Half) == QuarterTurns::Clockwise);
    REQUIRE(turned_counter_clockwise(QuarterTurns::Clockwise) == QuarterTurns::None);

    REQUIRE(turned_clockwise(turned_counter_clockwise(QuarterTurns::Clockwise)) ==
            QuarterTurns::Clockwise);
}

TEST_CASE("rotation: a clockwise rotation maps a rectangle", "[rotation]") {
    const ImageSize before{100, 50};
    const ImageRectangle rectangle{5, 7, 10, 20};

    // New x = before.height - y - height = 50 - 7 - 20 = 23; new y = x = 5;
    // width/height swap.
    REQUIRE(rotate_rectangle_clockwise(rectangle, before) == ImageRectangle{23, 5, 20, 10});
}

TEST_CASE("rotation: clockwise then counter-clockwise is the identity", "[rotation]") {
    const ImageSize before{100, 50};
    const ImageRectangle rectangle{5, 7, 10, 20};

    const ImageRectangle rotated = rotate_rectangle_clockwise(rectangle, before);
    const ImageSize after = rotated_size(before, QuarterTurns::Clockwise);

    REQUIRE(rotate_rectangle_counter_clockwise(rotated, after) == rectangle);
}

TEST_CASE("rotation: rotate_rectangle composes quarter turns", "[rotation]") {
    const ImageSize size{100, 50};
    const ImageRectangle rectangle{5, 7, 10, 20};

    REQUIRE(rotate_rectangle(rectangle, size, QuarterTurns::None) == rectangle);

    const ImageRectangle clockwise = rotate_rectangle_clockwise(rectangle, size);
    REQUIRE(rotate_rectangle(rectangle, size, QuarterTurns::Clockwise) == clockwise);

    const ImageSize after_clockwise = rotated_size(size, QuarterTurns::Clockwise);
    const ImageRectangle half = rotate_rectangle_clockwise(clockwise, after_clockwise);
    REQUIRE(rotate_rectangle(rectangle, size, QuarterTurns::Half) == half);

    REQUIRE(rotate_rectangle(clockwise, after_clockwise, QuarterTurns::CounterClockwise) ==
            rectangle);
}

TEST_CASE("rotation: inverse_quarter_turns inverts the state", "[rotation]") {
    REQUIRE(inverse_quarter_turns(QuarterTurns::None) == QuarterTurns::None);
    REQUIRE(inverse_quarter_turns(QuarterTurns::Half) == QuarterTurns::Half);
    REQUIRE(inverse_quarter_turns(QuarterTurns::Clockwise) == QuarterTurns::CounterClockwise);
    REQUIRE(inverse_quarter_turns(QuarterTurns::CounterClockwise) == QuarterTurns::Clockwise);
}
