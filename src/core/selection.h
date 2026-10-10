// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <algorithm>
#include <cmath>

#include "core/geometry.h"

namespace imagemgr {

/// The part of a selection a pointer is interacting with.
enum class SelectionHandle {
    None,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Inside,
};

namespace selection_detail {

inline double distance_squared(PixelPoint a, PixelPoint b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

/// Squared distance from point to the segment a-b (0 when inside the segment).
inline double distance_to_segment_squared(PixelPoint point, PixelPoint a, PixelPoint b) {
    const double ab_x = b.x - a.x;
    const double ab_y = b.y - a.y;
    const double length_squared = ab_x * ab_x + ab_y * ab_y;
    if (length_squared <= 0.0) {
        return distance_squared(point, a);
    }
    const double t = std::clamp(
        ((point.x - a.x) * ab_x + (point.y - a.y) * ab_y) / length_squared, 0.0, 1.0);
    const PixelPoint projection{a.x + t * ab_x, a.y + t * ab_y};
    return distance_squared(point, projection);
}

} // namespace selection_detail

/// Classifies point against region.
///
/// Corners are tested before edges (so a point near a corner reports the
/// corner), edges are tested as whole segments, and a point strictly inside
/// reports Inside. Anything further than handle_radius_px away from the border
/// and outside the rectangle reports None.
[[nodiscard]] inline SelectionHandle hit_test_selection(ImageRectangle region,
                                                        PixelPoint point,
                                                        double handle_radius_px) {
    const double radius_squared = handle_radius_px * handle_radius_px;

    const double left = region.x;
    const double top = region.y;
    const double right = region.x + region.width;
    const double bottom = region.y + region.height;

    const PixelPoint top_left{left, top};
    const PixelPoint top_right{right, top};
    const PixelPoint bottom_right{right, bottom};
    const PixelPoint bottom_left{left, bottom};

    if (selection_detail::distance_squared(point, top_left) <= radius_squared) {
        return SelectionHandle::TopLeft;
    }
    if (selection_detail::distance_squared(point, top_right) <= radius_squared) {
        return SelectionHandle::TopRight;
    }
    if (selection_detail::distance_squared(point, bottom_right) <= radius_squared) {
        return SelectionHandle::BottomRight;
    }
    if (selection_detail::distance_squared(point, bottom_left) <= radius_squared) {
        return SelectionHandle::BottomLeft;
    }

    if (selection_detail::distance_to_segment_squared(point, top_left, top_right) <=
        radius_squared) {
        return SelectionHandle::Top;
    }
    if (selection_detail::distance_to_segment_squared(point, top_right, bottom_right) <=
        radius_squared) {
        return SelectionHandle::Right;
    }
    if (selection_detail::distance_to_segment_squared(point, bottom_right, bottom_left) <=
        radius_squared) {
        return SelectionHandle::Bottom;
    }
    if (selection_detail::distance_to_segment_squared(point, bottom_left, top_left) <=
        radius_squared) {
        return SelectionHandle::Left;
    }

    if (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom) {
        return SelectionHandle::Inside;
    }
    return SelectionHandle::None;
}

/// Returns the region resized by dragging handle to point (both in image
/// coordinates), clamped to bounds.
///
/// Dragging an edge or corner past its opposite edge flips the rectangle rather
/// than producing a negative size. Inside/None leave the region unchanged.
[[nodiscard]] inline ImageRectangle resize_selection(ImageRectangle region,
                                                     SelectionHandle handle,
                                                     PixelPoint point,
                                                     ImageSize bounds) {
    double left = region.x;
    double top = region.y;
    double right = region.x + region.width;
    double bottom = region.y + region.height;

    const double point_x = std::clamp(point.x, 0.0, static_cast<double>(bounds.width));
    const double point_y = std::clamp(point.y, 0.0, static_cast<double>(bounds.height));

    switch (handle) {
    case SelectionHandle::TopLeft:
        left = point_x;
        top = point_y;
        break;
    case SelectionHandle::Top:
        top = point_y;
        break;
    case SelectionHandle::TopRight:
        right = point_x;
        top = point_y;
        break;
    case SelectionHandle::Right:
        right = point_x;
        break;
    case SelectionHandle::BottomRight:
        right = point_x;
        bottom = point_y;
        break;
    case SelectionHandle::Bottom:
        bottom = point_y;
        break;
    case SelectionHandle::BottomLeft:
        left = point_x;
        bottom = point_y;
        break;
    case SelectionHandle::Left:
        left = point_x;
        break;
    case SelectionHandle::Inside:
    case SelectionHandle::None:
        return region;
    }

    const ImageRectangle resized{
        .x = static_cast<int>(std::lround(std::min(left, right))),
        .y = static_cast<int>(std::lround(std::min(top, bottom))),
        .width = static_cast<int>(std::lround(std::abs(right - left))),
        .height = static_cast<int>(std::lround(std::abs(bottom - top))),
    };
    return clamp_rectangle(resized, bounds);
}

/// Returns the region translated by delta_px, clamped to bounds.
[[nodiscard]] inline ImageRectangle move_selection(ImageRectangle region,
                                                   PixelPoint delta_px,
                                                   ImageSize bounds) {
    const ImageRectangle moved{
        .x = static_cast<int>(std::lround(region.x + delta_px.x)),
        .y = static_cast<int>(std::lround(region.y + delta_px.y)),
        .width = region.width,
        .height = region.height,
    };
    return clamp_rectangle(moved, bounds);
}

} // namespace imagemgr
