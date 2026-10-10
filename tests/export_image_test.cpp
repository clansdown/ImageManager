// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include <gdkmm/pixbuf.h>

#include "core/export.h"
#include "io/export_image.h"
#include "io/image_loader.h"

using namespace imagemgr;

namespace {

std::string unique_temp_directory_path() {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    return (std::filesystem::temp_directory_path()
            / std::format("imagemgr-export-test-{}-{}", ::getpid(), index))
        .string();
}

class ScopedTempDirectory {
public:
    ScopedTempDirectory() : path_(unique_temp_directory_path()) {
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    ScopedTempDirectory(const ScopedTempDirectory&) = delete;
    ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::string path_;
};

Glib::RefPtr<Gdk::Pixbuf> make_test_pixbuf() {
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, 20, 10);
    pixbuf->fill(0x112233);
    return pixbuf;
}

} // namespace

TEST_CASE("export: crops the region and names it 1", "[export]") {
    ScopedTempDirectory directory;

    ExportRequest request;
    request.target_directory = directory.path();
    request.crop_region = ImageRectangle{5, 2, 8, 4};
    request.format = ImageFormat::Png;
    request.quality = 90;

    const auto result = export_region(make_test_pixbuf(), request);

    REQUIRE(result.has_value());
    REQUIRE(std::filesystem::path(*result).filename() == "1.png");
    REQUIRE(std::filesystem::exists(*result));

    const auto loaded = load_image(*result);
    REQUIRE(loaded.has_value());
    REQUIRE((*loaded)->get_width() == 8);
    REQUIRE((*loaded)->get_height() == 4);
}

TEST_CASE("export: picks the next sequential name", "[export]") {
    ScopedTempDirectory directory;
    std::ofstream(std::filesystem::path(directory.path()) / "1.png") << 'x';

    ExportRequest request;
    request.target_directory = directory.path();
    request.crop_region = ImageRectangle{0, 0, 4, 4};
    request.format = ImageFormat::Png;

    const auto result = export_region(make_test_pixbuf(), request);

    REQUIRE(result.has_value());
    REQUIRE(std::filesystem::path(*result).filename() == "2.png");
}

TEST_CASE("export: an empty region is an error", "[export]") {
    ScopedTempDirectory directory;

    ExportRequest request;
    request.target_directory = directory.path();
    request.crop_region = ImageRectangle{5, 5, 0, 0};

    REQUIRE_FALSE(export_region(make_test_pixbuf(), request).has_value());
}

TEST_CASE("export: a missing target directory is an error", "[export]") {
    ExportRequest request;
    request.crop_region = ImageRectangle{0, 0, 4, 4};

    REQUIRE_FALSE(export_region(make_test_pixbuf(), request).has_value());
}
