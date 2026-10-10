// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include <gdkmm/pixbuf.h>

#include "core/export.h"
#include "io/image_encoder.h"
#include "io/image_loader.h"

using namespace imagemgr;

namespace {

std::string unique_temp_path(std::string_view extension) {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    return (std::filesystem::temp_directory_path()
            / std::format("imagemgr-encoder-test-{}-{}.{}", ::getpid(), index, extension))
        .string();
}

Glib::RefPtr<Gdk::Pixbuf> make_test_pixbuf() {
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, true, 8, 16, 12);
    pixbuf->fill(0x3366ccff);
    return pixbuf;
}

void check_round_trip(ImageFormat format, std::string_view extension) {
    const std::string path = unique_temp_path(extension);

    const auto saved = save_image(make_test_pixbuf(), path, format, 90);
    REQUIRE(saved.has_value());

    const auto loaded = load_image(path);
    REQUIRE(loaded.has_value());
    REQUIRE((*loaded)->get_width() == 16);
    REQUIRE((*loaded)->get_height() == 12);

    std::error_code error;
    std::filesystem::remove(path, error);
}

} // namespace

TEST_CASE("image encoder: png round-trips", "[image-encoder]") {
    check_round_trip(ImageFormat::Png, "png");
}

TEST_CASE("image encoder: jpeg round-trips", "[image-encoder]") {
    check_round_trip(ImageFormat::Jpeg, "jpg");
}

TEST_CASE("image encoder: webp round-trips", "[image-encoder]") {
    check_round_trip(ImageFormat::Webp, "webp");
}

TEST_CASE("image encoder: avif round-trips", "[image-encoder]") {
    check_round_trip(ImageFormat::Avif, "avif");
}

TEST_CASE("image encoder: jpeg keeps colour when dropping alpha", "[image-encoder]") {
    const std::string path = unique_temp_path("jpg");

    // Opaque mid-blue. A broken alpha drop (copy_area across differing channel
    // counts) would leave the flattened buffer black instead.
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, true, 8, 16, 12);
    pixbuf->fill(0x3366ccff);

    const auto saved = save_image(pixbuf, path, ImageFormat::Jpeg, 90);
    REQUIRE(saved.has_value());

    const auto loaded = load_image(path);
    REQUIRE(loaded.has_value());

    const guint8* pixels = (*loaded)->get_pixels();
    const int channels = (*loaded)->get_n_channels();
    const int rowstride = (*loaded)->get_rowstride();
    const std::size_t offset = static_cast<std::size_t>(6) * static_cast<std::size_t>(rowstride) +
                               static_cast<std::size_t>(8) * static_cast<std::size_t>(channels);
    // JPEG is lossy; allow a generous tolerance but reject a black result.
    REQUIRE(pixels[offset + 0] > 0x10);
    REQUIRE(pixels[offset + 1] > 0x40);
    REQUIRE(pixels[offset + 2] > 0xa0);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("image encoder: jpeg flattens transparency onto white", "[image-encoder]") {
    const std::string path = unique_temp_path("jpg");

    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, true, 8, 16, 12);
    pixbuf->fill(0x00000000); // Fully transparent black.

    const auto saved = save_image(pixbuf, path, ImageFormat::Jpeg, 90);
    REQUIRE(saved.has_value());

    const auto loaded = load_image(path);
    REQUIRE(loaded.has_value());

    const guint8* pixels = (*loaded)->get_pixels();
    const int channels = (*loaded)->get_n_channels();
    const int rowstride = (*loaded)->get_rowstride();
    const std::size_t offset = static_cast<std::size_t>(6) * static_cast<std::size_t>(rowstride) +
                               static_cast<std::size_t>(8) * static_cast<std::size_t>(channels);
    // Transparent pixels should reveal the white background, not black.
    REQUIRE(pixels[offset + 0] > 0xd0);
    REQUIRE(pixels[offset + 1] > 0xd0);
    REQUIRE(pixels[offset + 2] > 0xd0);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("image encoder: a null pixbuf is an error", "[image-encoder]") {
    const auto saved = save_image({}, unique_temp_path("png"), ImageFormat::Png, 90);
    REQUIRE_FALSE(saved.has_value());
}
