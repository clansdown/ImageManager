// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "core/error.h"

namespace imagemgr {

/// Loads an image file into a pixbuf at full resolution.
///
/// Uses the gdk-pixbuf loaders, which cover jpeg, png, webp, avif, jxl and
/// more. Returns an Error if the file cannot be decoded.
///
/// Thread-safety: blocking decode; safe to call off the main thread (this is
/// how the thumbnail service uses it).
[[nodiscard]] std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> load_image(
    const std::string& file_path);

/// Loads an image scaled to fit within a max_dimension x max_dimension box,
/// preserving aspect ratio. Small images are not enlarged.
///
/// This is the thumbnail path and is cheaper than loading full-size. Returns an
/// Error if the file cannot be decoded.
///
/// Thread-safety: blocking decode; safe to call off the main thread.
[[nodiscard]] std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> load_image_scaled(
    const std::string& file_path, int max_dimension);

} // namespace imagemgr
