// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "core/error.h"
#include "core/rotation.h"

namespace imagemgr {

/// How a file's rotation can be persisted losslessly.
enum class RotationWriteMode {
    LosslessPixels,  // re-encode the rotated pixels in a lossless format
    ExifOrientation, // update the JPEG's EXIF orientation tag (pixels untouched)
    ViewOnly,        // the format cannot be written losslessly
};

/// The write strategy for file_path, chosen from its extension. JPEG uses the
/// EXIF orientation tag; PNG/BMP/TIFF/WebP are re-encoded losslessly; anything
/// else can only be rotated in the view.
[[nodiscard]] RotationWriteMode rotation_write_mode(const std::string& file_path);

/// Persists a rotation of file_path in the same format, losslessly.
///
/// turns is the rotation applied to the displayed image. For JPEG the EXIF
/// orientation tag is composed with turns and no pixels are touched; for the
/// lossless pixel formats rotated_pixbuf (the already-rotated image) is encoded
/// back. The write is atomic and preserves the file's permissions.
///
/// Thread-safety: blocking I/O and encoding; safe to call off the main thread.
[[nodiscard]] std::expected<void, Error> write_rotated_image(
    const std::string& file_path, QuarterTurns turns,
    const Glib::RefPtr<Gdk::Pixbuf>& rotated_pixbuf);

} // namespace imagemgr
