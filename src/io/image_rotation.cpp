// SPDX-License-Identifier: GPL-3.0-only

#include "io/image_rotation.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#include <giomm/file.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <webp/encode.h>

#include "io/exif_orientation.h"

namespace imagemgr {

namespace {

// The lowercased extension (without the dot), or empty when there is none.
std::string lowercased_extension(const std::string& file_path) {
    const std::size_t dot = file_path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string extension = file_path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension;
}

// The EXIF orientation value reached by rotating the displayed image by turns.
ExifOrientation compose_orientation(ExifOrientation current, QuarterTurns turns) {
    switch (turns) {
    case QuarterTurns::None:
        return current;
    case QuarterTurns::Clockwise:
        return orientation_after_clockwise(current);
    case QuarterTurns::CounterClockwise:
        return orientation_after_counter_clockwise(current);
    case QuarterTurns::Half:
        return orientation_after_clockwise(orientation_after_clockwise(current));
    }
    return current;
}

// Writes bytes to path atomically, preserving the original's permissions.
std::expected<void, Error> replace_file_contents(const std::string& path, const guint8* data,
                                                 std::size_t size) {
    try {
        const auto file = Gio::File::create_for_path(path);
        std::string etag;
        file->replace_contents(std::string(reinterpret_cast<const char*>(data), size),
                               std::string{}, etag);
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not write {}: {}", path, error.what())});
    }
    return {};
}

// PNG has a buffer saver, so it can be written without a temporary file.
std::expected<void, Error> write_png_losslessly(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                                const std::string& path) {
    gchar* buffer = nullptr;
    gsize size = 0;
    try {
        pixbuf->save_to_buffer(buffer, size, "png");
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not encode {}: {}", path, error.what())});
    }
    const auto written =
        replace_file_contents(path, reinterpret_cast<const guint8*>(buffer), size);
    g_free(buffer);
    return written;
}

// BMP and TIFF have no buffer saver: encode beside the file, then rename over
// it, preserving the original's mode.
std::expected<void, Error> write_via_temp_file(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                               const std::string& path,
                                               const Glib::ustring& type) {
    const std::string temporary = Glib::build_filename(
        Glib::path_get_dirname(path),
        "." + Glib::path_get_basename(path) + ".imagemgr-tmp");
    try {
        pixbuf->save(temporary, type);
    } catch (const Glib::Error& error) {
        g_unlink(temporary.c_str());
        return std::unexpected(
            Error{std::format("could not encode {}: {}", path, error.what())});
    }

    GStatBuf info{};
    if (g_stat(path.c_str(), &info) == 0) {
        g_chmod(temporary.c_str(), info.st_mode);
    }
    if (g_rename(temporary.c_str(), path.c_str()) != 0) {
        g_unlink(temporary.c_str());
        return std::unexpected(Error{std::format("could not replace {}", path)});
    }
    return {};
}

std::expected<void, Error> write_webp_losslessly(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                                 const std::string& path) {
    Glib::RefPtr<Gdk::Pixbuf> rgba = pixbuf;
    if (pixbuf->get_n_channels() != 4 || !pixbuf->get_has_alpha()) {
        rgba = pixbuf->add_alpha(false, 0, 0, 0);
    }
    const int width = rgba->get_width();
    const int height = rgba->get_height();
    const int stride = rgba->get_rowstride();

    uint8_t* output = nullptr;
    const std::size_t size =
        WebPEncodeLosslessRGBA(rgba->get_pixels(), width, height, stride, &output);
    if (size == 0 || output == nullptr) {
        if (output != nullptr) {
            WebPFree(output);
        }
        return std::unexpected(Error{std::format("could not encode {} as WebP", path)});
    }
    const auto written =
        replace_file_contents(path, reinterpret_cast<const guint8*>(output), size);
    WebPFree(output);
    return written;
}

} // namespace

RotationWriteMode rotation_write_mode(const std::string& file_path) {
    const std::string extension = lowercased_extension(file_path);
    if (extension == "jpg" || extension == "jpeg") {
        return RotationWriteMode::ExifOrientation;
    }
    if (extension == "png" || extension == "webp" || extension == "bmp" ||
        extension == "tif" || extension == "tiff") {
        return RotationWriteMode::LosslessPixels;
    }
    return RotationWriteMode::ViewOnly;
}

std::expected<void, Error> write_rotated_image(const std::string& file_path,
                                               QuarterTurns turns,
                                               const Glib::RefPtr<Gdk::Pixbuf>& rotated_pixbuf) {
    switch (rotation_write_mode(file_path)) {
    case RotationWriteMode::ExifOrientation: {
        const ExifOrientation current =
            read_jpeg_orientation(file_path).value_or(ExifOrientation::Normal);
        return write_jpeg_orientation(file_path, compose_orientation(current, turns));
    }
    case RotationWriteMode::LosslessPixels: {
        if (!rotated_pixbuf) {
            return std::unexpected(Error{"no rotated image to write"});
        }
        const std::string extension = lowercased_extension(file_path);
        if (extension == "png") {
            return write_png_losslessly(rotated_pixbuf, file_path);
        }
        if (extension == "webp") {
            return write_webp_losslessly(rotated_pixbuf, file_path);
        }
        if (extension == "bmp") {
            return write_via_temp_file(rotated_pixbuf, file_path, "bmp");
        }
        if (extension == "tif" || extension == "tiff") {
            return write_via_temp_file(rotated_pixbuf, file_path, "tiff");
        }
        return std::unexpected(Error{"unsupported lossless rotation format"});
    }
    case RotationWriteMode::ViewOnly:
        return std::unexpected(Error{"this format cannot be rotated losslessly"});
    }
    return std::unexpected(Error{"unsupported rotation"});
}

} // namespace imagemgr
