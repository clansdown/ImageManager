// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "core/error.h"
#include "core/export.h"

namespace imagemgr {

/// Writes a pixbuf to file_path in the given format.
///
/// PNG and JPEG go through gdk-pixbuf; WebP uses libwebp and AVIF uses libheif,
/// because gdk-pixbuf's WebP/HEIF loaders cannot write. PNG ignores quality.
///
/// Blocking: call off the main thread if the delay matters. Returns an Error if
/// the image cannot be encoded or the file cannot be written.
[[nodiscard]] std::expected<void, Error> save_image(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                                    const std::string& file_path,
                                                    ImageFormat format,
                                                    int quality);

} // namespace imagemgr
