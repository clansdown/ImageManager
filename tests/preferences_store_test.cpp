// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "io/preferences_store.h"

using namespace imagemgr;

namespace {

// Returns a process-unique path in the temp directory. The file is not created.
std::string unique_temp_file_path() {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    const auto path = std::filesystem::temp_directory_path()
        / std::format("imagemgr-preferences-test-{}-{}.ini", ::getpid(), index);
    return path.string();
}

// Owns a temp path and removes the file (if any) when destroyed.
class ScopedTempFile {
public:
    ScopedTempFile() : path_(unique_temp_file_path()) {}

    ~ScopedTempFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    ScopedTempFile(const ScopedTempFile&) = delete;
    ScopedTempFile& operator=(const ScopedTempFile&) = delete;

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace

TEST_CASE("preferences: a missing file yields the defaults", "[preferences]") {
    const std::string path = unique_temp_file_path();
    std::error_code error;
    std::filesystem::remove(path, error); // Ensure it really is absent.

    const auto loaded = load_preferences_from_file(path);

    REQUIRE(loaded.has_value());
    REQUIRE(*loaded == Preferences{});
}

TEST_CASE("preferences: values round-trip through the file", "[preferences]") {
    ScopedTempFile file;

    Preferences preferences;
    preferences.last_export_directory = "/tmp/exports";
    preferences.sidebar_width = 333;
    preferences.default_export_quality = 77;
    preferences.thumbnail_size = 192;
    preferences.sort_order = SortOrder::ModifiedDescending;
    preferences.window_width = 1000;
    preferences.window_height = 700;
    preferences.window_maximized = true;

    REQUIRE(save_preferences_to_file(file.path(), preferences).has_value());

    const auto loaded = load_preferences_from_file(file.path());
    REQUIRE(loaded.has_value());
    REQUIRE(*loaded == preferences);
}

TEST_CASE("preferences: an unknown sort order falls back to the default", "[preferences]") {
    ScopedTempFile file;
    {
        std::ofstream stream(file.path());
        stream << "[settings]\n";
        stream << "sort-order=banana\n";
    }

    const auto loaded = load_preferences_from_file(file.path());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->sort_order == SortOrder::NameAscending);
}

TEST_CASE("preferences: out-of-range values are clamped", "[preferences]") {
    ScopedTempFile file;

    Preferences preferences;
    preferences.default_export_quality = 500;
    preferences.thumbnail_size = 1;
    preferences.window_width = -5;

    REQUIRE(save_preferences_to_file(file.path(), preferences).has_value());

    const auto loaded = load_preferences_from_file(file.path());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->default_export_quality == kMaximumExportQuality);
    REQUIRE(loaded->thumbnail_size == kMinimumThumbnailSize);
    REQUIRE(loaded->window_width == 0);
}
