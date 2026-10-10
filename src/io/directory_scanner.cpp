// SPDX-License-Identifier: GPL-3.0-only

#include "io/directory_scanner.h"

#include <giomm/file.h>
#include <giomm/fileenumerator.h>
#include <giomm/fileinfo.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>
#include <unordered_set>

namespace imagemgr {

namespace {

// Central list of readable extensions. Adding a format here (plus its
// gdk-pixbuf loader) is all the scanner needs to start offering it.
const std::unordered_set<std::string>& supported_extensions() {
    static const std::unordered_set<std::string> extensions = {
        "jpg", "jpeg", "png", "webp", "avif", "jxl"};
    return extensions;
}

std::string ascii_lowercase(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

} // namespace

bool is_supported_image_file_name(std::string_view file_name) {
    const auto dot = file_name.rfind('.');
    if (dot == std::string_view::npos || dot + 1 >= file_name.size()) {
        return false;
    }
    return supported_extensions().contains(ascii_lowercase(file_name.substr(dot + 1)));
}

std::expected<std::vector<ImageFileEntry>, Error> scan_image_directory(
    const std::string& directory_path) {
    std::vector<ImageFileEntry> entries;

    try {
        const auto directory = Gio::File::create_for_path(directory_path);
        const auto enumerator = directory->enumerate_children(
            "standard::name,standard::display-name,standard::type,standard::is-hidden",
            Gio::FileQueryInfoFlags::NONE);

        while (true) {
            const auto info = enumerator->next_file();
            if (!info) {
                break;
            }
            if (info->get_file_type() != Gio::FileType::REGULAR || info->is_hidden()) {
                continue;
            }
            const std::string name = info->get_name();
            if (!is_supported_image_file_name(name)) {
                continue;
            }
            entries.push_back(ImageFileEntry{
                .path = Glib::build_filename(directory_path, name),
                .display_name = info->get_display_name(),
            });
        }
        enumerator->close();
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not read directory {}: {}", directory_path, error.what())});
    }

    std::ranges::sort(entries, {}, &ImageFileEntry::display_name);
    return entries;
}

std::expected<std::vector<std::string>, Error> list_directory_entry_names(
    const std::string& directory_path) {
    std::vector<std::string> names;

    try {
        const auto directory = Gio::File::create_for_path(directory_path);
        const auto enumerator = directory->enumerate_children(
            "standard::name,standard::type,standard::is-hidden",
            Gio::FileQueryInfoFlags::NONE);

        while (true) {
            const auto info = enumerator->next_file();
            if (!info) {
                break;
            }
            if (info->get_file_type() != Gio::FileType::REGULAR || info->is_hidden()) {
                continue;
            }
            names.push_back(info->get_name());
        }
        enumerator->close();
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not read directory {}: {}", directory_path, error.what())});
    }

    return names;
}

} // namespace imagemgr
