// SPDX-License-Identifier: GPL-3.0-only

#include "io/image_loader.h"

#include <format>

namespace imagemgr {

std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> load_image(const std::string& file_path) {
    try {
        return Gdk::Pixbuf::create_from_file(file_path);
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not load {}: {}", file_path, error.what())});
    }
}

std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> load_image_scaled(const std::string& file_path,
                                                                   int max_dimension) {
    try {
        return Gdk::Pixbuf::create_from_file(file_path, max_dimension, max_dimension, true);
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not load {}: {}", file_path, error.what())});
    }
}

} // namespace imagemgr
