// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace imagemgr {

/// EXIF orientation tag values (EXIF tag 0x0112). Each value names the transform
/// that maps the stored pixels to the upright image a viewer should show.
enum class ExifOrientation : int {
    Normal = 1,
    FlipHorizontal = 2,
    Rotate180 = 3,
    FlipVertical = 4,
    Transpose = 5,
    Rotate90 = 6,
    Transverse = 7,
    Rotate270 = 8,
};

/// The orientation value that results from rotating the *displayed* image one
/// quarter turn clockwise.
///
/// The stored pixels are unchanged, so the tag must be composed with the turn:
/// the new tag maps stored pixels to rot90(display).
[[nodiscard]] constexpr ExifOrientation orientation_after_clockwise(
    ExifOrientation orientation) {
    switch (orientation) {
    case ExifOrientation::Normal:
        return ExifOrientation::Rotate90;
    case ExifOrientation::Rotate90:
        return ExifOrientation::Rotate180;
    case ExifOrientation::Rotate180:
        return ExifOrientation::Rotate270;
    case ExifOrientation::Rotate270:
        return ExifOrientation::Normal;
    case ExifOrientation::FlipHorizontal:
        return ExifOrientation::Transpose;
    case ExifOrientation::Transpose:
        return ExifOrientation::FlipVertical;
    case ExifOrientation::FlipVertical:
        return ExifOrientation::Transverse;
    case ExifOrientation::Transverse:
        return ExifOrientation::FlipHorizontal;
    }
    return orientation;
}

/// The orientation value that results from rotating the displayed image one
/// quarter turn counter-clockwise.
[[nodiscard]] constexpr ExifOrientation orientation_after_counter_clockwise(
    ExifOrientation orientation) {
    switch (orientation) {
    case ExifOrientation::Normal:
        return ExifOrientation::Rotate270;
    case ExifOrientation::Rotate270:
        return ExifOrientation::Rotate180;
    case ExifOrientation::Rotate180:
        return ExifOrientation::Rotate90;
    case ExifOrientation::Rotate90:
        return ExifOrientation::Normal;
    case ExifOrientation::FlipHorizontal:
        return ExifOrientation::Transverse;
    case ExifOrientation::Transverse:
        return ExifOrientation::FlipVertical;
    case ExifOrientation::FlipVertical:
        return ExifOrientation::Transpose;
    case ExifOrientation::Transpose:
        return ExifOrientation::FlipHorizontal;
    }
    return orientation;
}

} // namespace imagemgr
