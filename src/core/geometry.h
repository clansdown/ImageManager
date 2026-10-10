// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace imagemgr {

/// Width/height of an image in pixels.
struct ImageSize {
    int width = 0;
    int height = 0;

    friend bool operator==(const ImageSize&, const ImageSize&) = default;
};

/// An axis-aligned rectangle in image pixel coordinates.
///
/// The rectangle covers the half-open region [x, x + width) x [y, y + height);
/// x/y is the top-left corner. A width or height of zero denotes an empty
/// rectangle.
struct ImageRectangle {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    friend bool operator==(const ImageRectangle&, const ImageRectangle&) = default;
};

/// A point in continuous (sub-pixel) coordinates, used for pointer positions.
struct PixelPoint {
    double x = 0.0;
    double y = 0.0;

    friend bool operator==(const PixelPoint&, const PixelPoint&) = default;
};

/// Clamps rectangle so that it lies entirely within [0, bounds).
///
/// The width and height are limited to the bounds first, then x/y are shifted
/// so the rectangle's far edges stay inside. A non-positive bounds produces an
/// empty rectangle. Negative input sizes are treated as zero.
[[nodiscard]] constexpr ImageRectangle clamp_rectangle(ImageRectangle rectangle,
                                                       ImageSize bounds) {
    if (bounds.width <= 0 || bounds.height <= 0) {
        return ImageRectangle{};
    }

    if (rectangle.width < 0) {
        rectangle.width = 0;
    }
    if (rectangle.height < 0) {
        rectangle.height = 0;
    }
    if (rectangle.width > bounds.width) {
        rectangle.width = bounds.width;
    }
    if (rectangle.height > bounds.height) {
        rectangle.height = bounds.height;
    }

    // Normalise an empty rectangle to the origin so callers can compare
    // directly.
    if (rectangle.width == 0 || rectangle.height == 0) {
        return ImageRectangle{};
    }

    if (rectangle.x < 0) {
        rectangle.x = 0;
    }
    if (rectangle.y < 0) {
        rectangle.y = 0;
    }
    if (rectangle.x > bounds.width - rectangle.width) {
        rectangle.x = bounds.width - rectangle.width;
    }
    if (rectangle.y > bounds.height - rectangle.height) {
        rectangle.y = bounds.height - rectangle.height;
    }

    return rectangle;
}

} // namespace imagemgr
