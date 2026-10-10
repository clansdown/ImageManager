// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "core/error.h"
#include "core/export.h"

namespace imagemgr {

/// Crops a display pixbuf and writes it to a sequentially named file.
///
/// The crop region in request.crop_region is clamped to the pixbuf bounds, the
/// next free sequential name is chosen in request.target_directory, and the
/// region is encoded there. Returns the full path of the file that was written,
/// or an Error if the region is empty, the directory cannot be read, or
/// encoding fails.
///
/// Blocking: call off the main thread if the delay matters.
[[nodiscard]] std::expected<std::string, Error> export_region(
    const Glib::RefPtr<Gdk::Pixbuf>& display_pixbuf, const ExportRequest& request);

} // namespace imagemgr
