// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

#include "core/sort_order.h"

namespace imagemgr {

/// Valid range for Preferences::default_export_quality (percent).
inline constexpr int kMinimumExportQuality = 0;
inline constexpr int kMaximumExportQuality = 100;

/// Valid range for Preferences::thumbnail_size (logical pixels).
inline constexpr int kMinimumThumbnailSize = 16;
inline constexpr int kMaximumThumbnailSize = 512;

/// User-configurable application settings and their defaults.
///
/// A plain, throw-free value type: it carries no I/O and no widget state, so it
/// can be copied, compared and unit-tested freely. Persistence lives in
/// io/preferences_store.h; the in-memory value is the source of truth while the
/// application runs.
struct Preferences {
    /// Directory the most recent export was written to, or empty when no export
    /// has happened yet. Used as the default target directory for the next
    /// export.
    std::string last_export_directory;

    /// Width in logical pixels of the directory pane.
    int sidebar_width = 280;

    /// Encoder quality (0-100) for lossy export formats; ignored by lossless
    /// formats such as PNG.
    int default_export_quality = 90;

    /// Edge length in logical pixels of directory-list thumbnails.
    int thumbnail_size = 128;

    /// Whether directory-list cells show the file name under the thumbnail.
    bool show_filenames_in_list = true;

    /// Order in which the directory grid lists images.
    SortOrder sort_order = SortOrder::NameAscending;

    /// Size of the main window in logical pixels when last closed. Zero means
    /// "use the built-in default".
    int window_width = 0;
    int window_height = 0;

    /// Whether the main window was maximized when last closed.
    bool window_maximized = false;

    friend bool operator==(const Preferences&, const Preferences&) = default;
};

/// Returns a copy with every field clamped into its valid range.
///
/// Used on load and before save so a hand-edited or corrupt settings file can
/// never put the UI into an invalid state (negative sizes, 500% quality, ...).
[[nodiscard]] inline Preferences clamped_to_valid_ranges(Preferences preferences) {
    if (preferences.sidebar_width < 0) {
        preferences.sidebar_width = 0;
    }
    if (preferences.default_export_quality < kMinimumExportQuality) {
        preferences.default_export_quality = kMinimumExportQuality;
    } else if (preferences.default_export_quality > kMaximumExportQuality) {
        preferences.default_export_quality = kMaximumExportQuality;
    }
    if (preferences.thumbnail_size < kMinimumThumbnailSize) {
        preferences.thumbnail_size = kMinimumThumbnailSize;
    } else if (preferences.thumbnail_size > kMaximumThumbnailSize) {
        preferences.thumbnail_size = kMaximumThumbnailSize;
    }
    if (preferences.window_width < 0) {
        preferences.window_width = 0;
    }
    if (preferences.window_height < 0) {
        preferences.window_height = 0;
    }
    return preferences;
}

} // namespace imagemgr
