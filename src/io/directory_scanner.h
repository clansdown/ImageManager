// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.h"

namespace imagemgr {

/// A regular image file found in a directory.
struct ImageFileEntry {
    /// Full path to the file.
    std::string path;

    /// Human-readable name for display (may be localised).
    std::string display_name;
};

/// Returns true when file_name carries an extension this application can read.
/// The comparison is case-insensitive; the supported set is jpg, jpeg, png,
/// webp, avif and jxl.
[[nodiscard]] bool is_supported_image_file_name(std::string_view file_name);

/// Scans a single directory (non-recursive) for regular, non-hidden files whose
/// extension is supported, returned sorted by display name.
///
/// Uses Gio for I/O. This call blocks while enumerating, so run it off the main
/// thread when the directory may be large. Returns an Error if the directory
/// cannot be opened or read.
[[nodiscard]] std::expected<std::vector<ImageFileEntry>, Error> scan_image_directory(
    const std::string& directory_path);

/// Returns the base names (including extensions) of the regular, non-hidden
/// files directly inside directory_path, in enumeration order.
///
/// Used for export naming, which considers every file rather than only images.
/// Returns an Error if the directory cannot be opened.
[[nodiscard]] std::expected<std::vector<std::string>, Error> list_directory_entry_names(
    const std::string& directory_path);

} // namespace imagemgr
