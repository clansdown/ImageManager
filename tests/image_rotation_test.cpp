// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include <gdkmm/pixbuf.h>

#include "io/exif_orientation.h"
#include "io/image_rotation.h"

using namespace imagemgr;

namespace {

std::string unique_path(std::string_view extension) {
    static std::atomic<int> counter{0};
    return (std::filesystem::temp_directory_path() /
            std::format("imagemgr-rotate-{}-{}.{}", ::getpid(), counter.fetch_add(1), extension))
        .string();
}

Glib::RefPtr<Gdk::Pixbuf> make_pixbuf(int width, int height) {
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, width, height);
    pixbuf->fill(0x3366ccff);
    return pixbuf;
}

} // namespace

TEST_CASE("image rotation: write mode follows the extension", "[image-rotation]") {
    REQUIRE(rotation_write_mode("/tmp/a.png") == RotationWriteMode::LosslessPixels);
    REQUIRE(rotation_write_mode("/tmp/a.bmp") == RotationWriteMode::LosslessPixels);
    REQUIRE(rotation_write_mode("/tmp/a.TIF") == RotationWriteMode::LosslessPixels);
    REQUIRE(rotation_write_mode("/tmp/a.tiff") == RotationWriteMode::LosslessPixels);
    REQUIRE(rotation_write_mode("/tmp/a.webp") == RotationWriteMode::LosslessPixels);
    REQUIRE(rotation_write_mode("/tmp/a.jpg") == RotationWriteMode::ExifOrientation);
    REQUIRE(rotation_write_mode("/tmp/a.JPEG") == RotationWriteMode::ExifOrientation);
    REQUIRE(rotation_write_mode("/tmp/a.gif") == RotationWriteMode::ViewOnly);
    REQUIRE(rotation_write_mode("/tmp/a.avif") == RotationWriteMode::ViewOnly);
    REQUIRE(rotation_write_mode("/tmp/a") == RotationWriteMode::ViewOnly);
}

TEST_CASE("image rotation: PNG is rewritten losslessly", "[image-rotation]") {
    const std::string path = unique_path("png");
    make_pixbuf(16, 12)->save(path, "png");

    // Rotating a 16x12 image clockwise yields 12x16.
    REQUIRE(write_rotated_image(path, QuarterTurns::Clockwise, make_pixbuf(12, 16)));

    const auto loaded = Gdk::Pixbuf::create_from_file(path);
    REQUIRE(loaded->get_width() == 12);
    REQUIRE(loaded->get_height() == 16);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("image rotation: view-only formats report an error", "[image-rotation]") {
    const std::string path = unique_path("gif");
    const auto result = write_rotated_image(path, QuarterTurns::Clockwise, make_pixbuf(4, 4));
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("image rotation: JPEG rotation composes the EXIF orientation", "[image-rotation]") {
    const std::string path = unique_path("jpg");
    make_pixbuf(20, 10)->save(path, "jpeg");
    REQUIRE_FALSE(read_jpeg_orientation(path).has_value());

    REQUIRE(write_rotated_image(path, QuarterTurns::Clockwise, nullptr));
    REQUIRE(read_jpeg_orientation(path) == ExifOrientation::Rotate90);

    REQUIRE(write_rotated_image(path, QuarterTurns::Clockwise, nullptr));
    REQUIRE(read_jpeg_orientation(path) == ExifOrientation::Rotate180);

    REQUIRE(write_rotated_image(path, QuarterTurns::CounterClockwise, nullptr));
    REQUIRE(read_jpeg_orientation(path) == ExifOrientation::Rotate90);

    // The pixels are untouched throughout.
    const auto loaded = Gdk::Pixbuf::create_from_file(path);
    REQUIRE(loaded->get_width() == 20);
    REQUIRE(loaded->get_height() == 10);

    std::error_code error;
    std::filesystem::remove(path, error);
}
