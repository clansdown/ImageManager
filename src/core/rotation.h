// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/geometry.h"

namespace imagemgr {

/// Non-destructive rotation state, always a multiple of 90 degrees.
///
/// The source image is never modified; the document derives a rotated view from
/// this value. The enumeration is ordered so that turning clockwise advances by
/// one step and wraps around.
enum class QuarterTurns {
    None,
    Clockwise,
    Half,
    CounterClockwise,
};

/// Returns the size of an image after applying turns.
[[nodiscard]] constexpr ImageSize rotated_size(ImageSize size, QuarterTurns turns) {
    switch (turns) {
    case QuarterTurns::None:
    case QuarterTurns::Half:
        return size;
    case QuarterTurns::Clockwise:
    case QuarterTurns::CounterClockwise:
        return ImageSize{.width = size.height, .height = size.width};
    }
    return size;
}

/// Returns the state reached by rotating one more quarter turn clockwise.
[[nodiscard]] constexpr QuarterTurns turned_clockwise(QuarterTurns turns) {
    switch (turns) {
    case QuarterTurns::None:
        return QuarterTurns::Clockwise;
    case QuarterTurns::Clockwise:
        return QuarterTurns::Half;
    case QuarterTurns::Half:
        return QuarterTurns::CounterClockwise;
    case QuarterTurns::CounterClockwise:
        return QuarterTurns::None;
    }
    return QuarterTurns::None;
}

/// Returns the state reached by rotating one more quarter turn counter-clockwise.
[[nodiscard]] constexpr QuarterTurns turned_counter_clockwise(QuarterTurns turns) {
    switch (turns) {
    case QuarterTurns::None:
        return QuarterTurns::CounterClockwise;
    case QuarterTurns::CounterClockwise:
        return QuarterTurns::Half;
    case QuarterTurns::Half:
        return QuarterTurns::Clockwise;
    case QuarterTurns::Clockwise:
        return QuarterTurns::None;
    }
    return QuarterTurns::None;
}

/// Maps a rectangle from an image of size before to the same rectangle after
/// the image is rotated 90 degrees clockwise.
///
/// The result is expressed in the rotated frame, whose size is
/// rotated_size(before, QuarterTurns::Clockwise).
[[nodiscard]] constexpr ImageRectangle rotate_rectangle_clockwise(ImageRectangle rectangle,
                                                                  ImageSize before) {
    return ImageRectangle{
        .x = before.height - rectangle.y - rectangle.height,
        .y = rectangle.x,
        .width = rectangle.height,
        .height = rectangle.width,
    };
}

/// Maps a rectangle from an image of size before to the same rectangle after
/// the image is rotated 90 degrees counter-clockwise.
///
/// The result is expressed in the rotated frame, whose size is
/// rotated_size(before, QuarterTurns::CounterClockwise).
[[nodiscard]] constexpr ImageRectangle rotate_rectangle_counter_clockwise(
    ImageRectangle rectangle, ImageSize before) {
    return ImageRectangle{
        .x = rectangle.y,
        .y = before.width - rectangle.x - rectangle.width,
        .width = rectangle.height,
        .height = rectangle.width,
    };
}

} // namespace imagemgr
