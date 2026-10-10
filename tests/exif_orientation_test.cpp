// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <string>

#include <gdkmm/pixbuf.h>

#include "io/exif_orientation.h"

using namespace imagemgr;

namespace {

std::string unique_jpeg_path() {
    static std::atomic<int> counter{0};
    return (std::filesystem::temp_directory_path() /
            std::format("imagemgr-exif-{}-{}.jpg", ::getpid(), counter.fetch_add(1)))
        .string();
}

} // namespace

TEST_CASE("exif orientation: write then read round-trips without touching pixels",
          "[exif-orientation]") {
    const std::string path = unique_jpeg_path();

    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, 20, 10);
    pixbuf->fill(0x3366ccff);
    pixbuf->save(path, "jpeg");

    // A freshly encoded JPEG carries no EXIF orientation.
    REQUIRE_FALSE(read_jpeg_orientation(path).has_value());

    REQUIRE(write_jpeg_orientation(path, ExifOrientation::Rotate90));
    const auto read_back = read_jpeg_orientation(path);
    REQUIRE(read_back.has_value());
    REQUIRE(*read_back == ExifOrientation::Rotate90);

    // The compressed pixels are copied verbatim: dimensions are unchanged.
    const auto loaded = Gdk::Pixbuf::create_from_file(path);
    REQUIRE(loaded->get_width() == 20);
    REQUIRE(loaded->get_height() == 10);

    // Writing again updates the tag.
    REQUIRE(write_jpeg_orientation(path, ExifOrientation::Rotate180));
    REQUIRE(*read_jpeg_orientation(path) == ExifOrientation::Rotate180);

    std::error_code error;
    std::filesystem::remove(path, error);
}
