// SPDX-License-Identifier: GPL-3.0-only

#include "io/export_image.h"

#include <format>
#include <string>

#include <glibmm/miscutils.h>

#include "core/sequence_naming.h"
#include "io/directory_scanner.h"
#include "io/image_encoder.h"

namespace imagemgr {

std::expected<std::string, Error> export_region(
    const Glib::RefPtr<Gdk::Pixbuf>& display_pixbuf, const ExportRequest& request) {
    if (!display_pixbuf) {
        return std::unexpected(Error{"there is no image to export"});
    }
    if (request.target_directory.empty()) {
        return std::unexpected(Error{"no export directory has been chosen"});
    }

    const ImageSize bounds{.width = display_pixbuf->get_width(),
                           .height = display_pixbuf->get_height()};
    const ImageRectangle region = clamp_rectangle(request.crop_region, bounds);
    if (region.width <= 0 || region.height <= 0) {
        return std::unexpected(Error{"the selected region is empty"});
    }

    // create_subpixbuf is the one call here that can throw (gtkmm surfaces a
    // GError as an exception); catch it so no exception can escape a caller such
    // as a GTK async callback, where it would terminate the process.
    Glib::RefPtr<Gdk::Pixbuf> cropped;
    try {
        cropped = Gdk::Pixbuf::create_subpixbuf(display_pixbuf, region.x, region.y,
                                                region.width, region.height);
    } catch (const Glib::Error& error) {
        return std::unexpected(Error{std::format("could not crop the image: {}", error.what())});
    }

    const auto entry_names = list_directory_entry_names(request.target_directory);
    if (!entry_names) {
        return std::unexpected(entry_names.error());
    }

    const std::string extension{extension_for_format(request.format)};
    const std::string file_name = next_sequential_filename(*entry_names, extension);
    const std::string file_path = Glib::build_filename(request.target_directory, file_name);

    const auto saved = save_image(cropped, file_path, request.format, request.quality);
    if (!saved) {
        return std::unexpected(saved.error());
    }

    return file_path;
}

} // namespace imagemgr
