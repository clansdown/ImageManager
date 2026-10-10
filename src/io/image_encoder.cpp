// SPDX-License-Identifier: GPL-3.0-only

#include "io/image_encoder.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <memory>
#include <string>

#include <giomm/file.h>
#include <libheif/heif.h>
#include <webp/encode.h>

namespace imagemgr {

namespace {

// Returns an RGBA pixbuf: the input when it already has an alpha channel,
// otherwise a new pixbuf with alpha added. Used by the WebP/AVIF encoders,
// which want 4-channel interleaved data.
Glib::RefPtr<Gdk::Pixbuf> ensure_rgba(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) {
    if (pixbuf->get_n_channels() == 4 && pixbuf->get_has_alpha()) {
        return pixbuf;
    }
    return pixbuf->add_alpha(false, 0, 0, 0);
}

// Returns an RGB pixbuf with any alpha channel composited over an opaque white
// background. The JPEG saver cannot take an alpha channel. gdk-pixbuf's
// copy_area requires matching channel counts, so it cannot be used to drop
// alpha; composite() blends correctly instead.
Glib::RefPtr<Gdk::Pixbuf> flatten_to_rgb(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) {
    if (!pixbuf->get_has_alpha()) {
        return pixbuf;
    }
    const int width = pixbuf->get_width();
    const int height = pixbuf->get_height();
    auto rgb = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, width, height);
    rgb->fill(0xffffffff); // Opaque white, showing through any transparency.
    pixbuf->composite(rgb, 0, 0, width, height, 0.0, 0.0, 1.0, 1.0, Gdk::InterpType::NEAREST, 255);
    return rgb;
}

// Writes raw bytes to path via Gio, replacing any existing file.
std::expected<void, Error> write_bytes(const std::string& file_path, const guint8* data,
                                       std::size_t size) {
    try {
        const auto file = Gio::File::create_for_path(file_path);
        std::string new_etag;
        file->replace_contents(
            std::string(reinterpret_cast<const char*>(data), size), std::string{}, new_etag);
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not write {}: {}", file_path, error.what())});
    }
    return {};
}

std::expected<void, Error> encode_via_gdk_pixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                                 const std::string& file_path,
                                                 const Glib::ustring& type, int quality) {
    try {
        if (type == "jpeg") {
            flatten_to_rgb(pixbuf)->save(file_path, type, {"quality"},
                                         {std::to_string(quality)});
        } else {
            pixbuf->save(file_path, type);
        }
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not write {}: {}", file_path, error.what())});
    }
    return {};
}

std::expected<void, Error> encode_webp(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                       const std::string& file_path, int quality) {
    const auto rgba = ensure_rgba(pixbuf);
    const int width = rgba->get_width();
    const int height = rgba->get_height();
    const int stride = rgba->get_rowstride();
    const guint8* pixels = rgba->get_pixels();

    const int clamped_quality = std::clamp(quality, 0, 100);

    uint8_t* output = nullptr;
    std::size_t size = 0;
    if (clamped_quality >= 100) {
        size = WebPEncodeLosslessRGBA(pixels, width, height, stride, &output);
    } else {
        size = WebPEncodeRGBA(pixels, width, height, stride,
                              static_cast<float>(clamped_quality), &output);
    }

    if (size == 0 || output == nullptr) {
        if (output != nullptr) {
            WebPFree(output);
        }
        return std::unexpected(Error{std::format("could not encode {} as WebP", file_path)});
    }

    const auto written = write_bytes(file_path, reinterpret_cast<const guint8*>(output), size);
    WebPFree(output);
    return written;
}

struct HeifContextDeleter {
    void operator()(heif_context* context) const noexcept { heif_context_free(context); }
};
struct HeifImageDeleter {
    void operator()(heif_image* image) const noexcept { heif_image_release(image); }
};
struct HeifEncoderDeleter {
    void operator()(heif_encoder* encoder) const noexcept { heif_encoder_release(encoder); }
};
struct HeifHandleDeleter {
    void operator()(heif_image_handle* handle) const noexcept { heif_image_handle_release(handle); }
};

std::string heif_error_text(const heif_error& error) {
    return error.message != nullptr ? std::string(error.message) : std::string("unknown error");
}

std::expected<void, Error> encode_avif(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                       const std::string& file_path, int quality) {
    const auto rgba = ensure_rgba(pixbuf);
    const int width = rgba->get_width();
    const int height = rgba->get_height();
    const int stride = rgba->get_rowstride();
    const guint8* pixels = rgba->get_pixels();

    std::unique_ptr<heif_context, HeifContextDeleter> context(heif_context_alloc());
    if (!context) {
        return std::unexpected(Error{"could not create a HEIF context"});
    }

    heif_encoder* raw_encoder = nullptr;
    heif_error error =
        heif_context_get_encoder_for_format(context.get(), heif_compression_AV1, &raw_encoder);
    if (error.code != heif_error_Ok) {
        return std::unexpected(
            Error{std::format("no AVIF encoder available: {}", heif_error_text(error))});
    }
    std::unique_ptr<heif_encoder, HeifEncoderDeleter> encoder(raw_encoder);
    heif_encoder_set_lossy_quality(encoder.get(), std::clamp(quality, 0, 100));

    heif_image* raw_image = nullptr;
    error = heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RGBA,
                              &raw_image);
    if (error.code != heif_error_Ok) {
        return std::unexpected(
            Error{std::format("could not create AVIF image: {}", heif_error_text(error))});
    }
    std::unique_ptr<heif_image, HeifImageDeleter> image(raw_image);

    error = heif_image_add_plane(image.get(), heif_channel_interleaved, width, height, 8);
    if (error.code != heif_error_Ok) {
        return std::unexpected(
            Error{std::format("could not allocate AVIF plane: {}", heif_error_text(error))});
    }

    int plane_stride = 0;
    uint8_t* plane = heif_image_get_plane(image.get(), heif_channel_interleaved, &plane_stride);
    if (plane == nullptr) {
        return std::unexpected(Error{"could not access the AVIF image plane"});
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(plane + static_cast<std::ptrdiff_t>(y) * plane_stride,
                    pixels + static_cast<std::ptrdiff_t>(y) * stride,
                    static_cast<std::size_t>(width) * 4);
    }

    heif_image_handle* raw_handle = nullptr;
    error = heif_context_encode_image(context.get(), image.get(), encoder.get(), nullptr,
                                      &raw_handle);
    if (error.code != heif_error_Ok) {
        return std::unexpected(
            Error{std::format("could not encode AVIF: {}", heif_error_text(error))});
    }
    std::unique_ptr<heif_image_handle, HeifHandleDeleter> handle(raw_handle);

    error = heif_context_write_to_file(context.get(), file_path.c_str());
    if (error.code != heif_error_Ok) {
        return std::unexpected(
            Error{std::format("could not write {}: {}", file_path, heif_error_text(error))});
    }
    return {};
}

} // namespace

std::expected<void, Error> save_image(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                      const std::string& file_path, ImageFormat format,
                                      int quality) {
    if (!pixbuf) {
        return std::unexpected(Error{"no image to save"});
    }

    switch (format) {
    case ImageFormat::Png:
        return encode_via_gdk_pixbuf(pixbuf, file_path, "png", quality);
    case ImageFormat::Jpeg:
        return encode_via_gdk_pixbuf(pixbuf, file_path, "jpeg", quality);
    case ImageFormat::Webp:
        return encode_webp(pixbuf, file_path, quality);
    case ImageFormat::Avif:
        return encode_avif(pixbuf, file_path, quality);
    }
    return std::unexpected(Error{"unsupported export format"});
}

} // namespace imagemgr
