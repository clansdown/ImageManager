// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <optional>
#include <string>

#include "core/error.h"
#include "core/orientation.h"

namespace imagemgr {

/// Reads a JPEG's EXIF orientation tag, or nullopt when the file has no EXIF
/// orientation (or cannot be read).
///
/// Thread-safety: blocking file I/O; safe to call off the main thread.
[[nodiscard]] std::optional<ExifOrientation> read_jpeg_orientation(
    const std::string& file_path);

/// Writes a JPEG's EXIF orientation tag without touching the compressed pixel
/// data: the DCT coefficients are copied verbatim and only the APP1/Exif marker
/// is replaced (created if absent). Other metadata markers are preserved.
///
/// The write is atomic (a temporary file replaces the original) and preserves
/// the original file's permissions.
///
/// Thread-safety: blocking file I/O; safe to call off the main thread.
[[nodiscard]] std::expected<void, Error> write_jpeg_orientation(
    const std::string& file_path, ExifOrientation orientation);

} // namespace imagemgr
