// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "io/directory_scanner.h"

using namespace imagemgr;

namespace {

std::string unique_temp_directory_path() {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    return (std::filesystem::temp_directory_path()
            / std::format("imagemgr-scanner-test-{}-{}", ::getpid(), index))
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

void create_file(const std::string& directory, const std::string& name) {
    std::ofstream stream(std::filesystem::path(directory) / name);
    stream << 'x';
}

} // namespace

TEST_CASE("scanner: recognises supported image extensions", "[directory-scanner]") {
    REQUIRE(is_supported_image_file_name("photo.jpg"));
    REQUIRE(is_supported_image_file_name("photo.JPEG"));
    REQUIRE(is_supported_image_file_name("a.png"));
    REQUIRE(is_supported_image_file_name("a.webp"));
    REQUIRE(is_supported_image_file_name("a.avif"));
    REQUIRE(is_supported_image_file_name("a.jxl"));

    REQUIRE_FALSE(is_supported_image_file_name("notes.txt"));
    REQUIRE_FALSE(is_supported_image_file_name("noextension"));
    REQUIRE_FALSE(is_supported_image_file_name("trailing."));
}

TEST_CASE("scanner: returns supported regular files sorted by name", "[directory-scanner]") {
    ScopedTempDirectory directory;
    create_file(directory.path(), "b.png");
    create_file(directory.path(), "a.JPG");
    create_file(directory.path(), "notes.txt");
    create_file(directory.path(), "c.webp");
    create_file(directory.path(), ".hidden.png");

    const auto scanned = scan_image_directory(directory.path());

    REQUIRE(scanned.has_value());
    REQUIRE(scanned->size() == 3);
    REQUIRE((*scanned)[0].display_name == "a.JPG");
    REQUIRE((*scanned)[1].display_name == "b.png");
    REQUIRE((*scanned)[2].display_name == "c.webp");
    REQUIRE((*scanned)[0].path == (std::filesystem::path(directory.path()) / "a.JPG").string());
}

TEST_CASE("scanner: an unreadable directory is an error", "[directory-scanner]") {
    const auto scanned = scan_image_directory("/nonexistent/imagemgr-scanner-directory");

    REQUIRE_FALSE(scanned.has_value());
    REQUIRE_FALSE(scanned.error().message.empty());
}
