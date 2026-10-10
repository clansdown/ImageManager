// SPDX-License-Identifier: GPL-3.0-only

#include "io/preferences_store.h"

#include <glib.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

#include <filesystem>
#include <format>
#include <system_error>

namespace imagemgr {

namespace {

// Keys and groups used in settings.ini.
constexpr auto kSettingsGroup = "settings";
constexpr auto kLastExportDirectoryKey = "last-export-directory";
constexpr auto kSidebarWidthKey = "sidebar-width";
constexpr auto kDefaultExportQualityKey = "default-export-quality";
constexpr auto kThumbnailSizeKey = "thumbnail-size";
constexpr auto kSortOrderKey = "sort-order";
constexpr auto kWindowWidthKey = "window-width";
constexpr auto kWindowHeightKey = "window-height";
constexpr auto kWindowMaximizedKey = "window-maximized";

// Reads a value only when the key is present, leaving the default untouched
// otherwise. A malformed value (for example a non-numeric integer) is ignored
// with a warning rather than failing the whole load.
void read_string(const Glib::KeyFile& key_file, const char* key, std::string& target) {
    if (!key_file.has_key(kSettingsGroup, key)) {
        return;
    }
    try {
        target = key_file.get_string(kSettingsGroup, key);
    } catch (const Glib::Error& error) {
        g_warning("ignoring preferences key '%s': %s", key, error.what());
    }
}

void read_int(const Glib::KeyFile& key_file, const char* key, int& target) {
    if (!key_file.has_key(kSettingsGroup, key)) {
        return;
    }
    try {
        target = key_file.get_integer(kSettingsGroup, key);
    } catch (const Glib::Error& error) {
        g_warning("ignoring preferences key '%s': %s", key, error.what());
    }
}

void read_bool(const Glib::KeyFile& key_file, const char* key, bool& target) {
    if (!key_file.has_key(kSettingsGroup, key)) {
        return;
    }
    try {
        target = key_file.get_boolean(kSettingsGroup, key);
    } catch (const Glib::Error& error) {
        g_warning("ignoring preferences key '%s': %s", key, error.what());
    }
}

// An unknown sort-order value is ignored with a warning, leaving the default in
// place, so a hand-edited or forward-versioned file cannot break loading.
void read_sort_order(const Glib::KeyFile& key_file, const char* key, SortOrder& target) {
    if (!key_file.has_key(kSettingsGroup, key)) {
        return;
    }
    try {
        const std::string value = key_file.get_string(kSettingsGroup, key);
        if (const auto parsed = sort_order_from_string(value)) {
            target = *parsed;
        } else {
            g_warning("ignoring preferences key '%s': unknown sort order '%s'", key, value.c_str());
        }
    } catch (const Glib::Error& error) {
        g_warning("ignoring preferences key '%s': %s", key, error.what());
    }
}

} // namespace

std::string default_preferences_file_path() {
    return Glib::build_filename(Glib::get_user_config_dir(), "ImageManager", "settings.ini");
}

std::expected<Preferences, Error> load_preferences_from_file(const std::string& file_path) {
    Preferences preferences;

    std::error_code exists_error;
    if (!std::filesystem::exists(file_path, exists_error)) {
        // First run (or the file was removed): defaults are the correct answer.
        return preferences;
    }

    const auto key_file = Glib::KeyFile::create();
    try {
        if (!key_file->load_from_file(file_path)) {
            return std::unexpected(
                Error{std::format("could not read preferences from {}", file_path)});
        }
    } catch (const Glib::Error& error) {
        return std::unexpected(Error{
            std::format("could not read preferences from {}: {}", file_path, error.what())});
    }

    read_string(*key_file, kLastExportDirectoryKey, preferences.last_export_directory);
    read_int(*key_file, kSidebarWidthKey, preferences.sidebar_width);
    read_int(*key_file, kDefaultExportQualityKey, preferences.default_export_quality);
    read_int(*key_file, kThumbnailSizeKey, preferences.thumbnail_size);
    read_sort_order(*key_file, kSortOrderKey, preferences.sort_order);
    read_int(*key_file, kWindowWidthKey, preferences.window_width);
    read_int(*key_file, kWindowHeightKey, preferences.window_height);
    read_bool(*key_file, kWindowMaximizedKey, preferences.window_maximized);

    return clamped_to_valid_ranges(preferences);
}

std::expected<void, Error> save_preferences_to_file(const std::string& file_path,
                                                    const Preferences& preferences) {
    const Preferences valid = clamped_to_valid_ranges(preferences);

    const std::filesystem::path path(file_path);
    if (path.has_parent_path()) {
        std::error_code directory_error;
        std::filesystem::create_directories(path.parent_path(), directory_error);
        if (directory_error) {
            return std::unexpected(Error{std::format("could not create {}: {}", path.parent_path().string(),
                                                     directory_error.message())});
        }
    }

    const auto key_file = Glib::KeyFile::create();
    key_file->set_string(kSettingsGroup, kLastExportDirectoryKey, valid.last_export_directory);
    key_file->set_integer(kSettingsGroup, kSidebarWidthKey, valid.sidebar_width);
    key_file->set_integer(kSettingsGroup, kDefaultExportQualityKey, valid.default_export_quality);
    key_file->set_integer(kSettingsGroup, kThumbnailSizeKey, valid.thumbnail_size);
    key_file->set_string(kSettingsGroup, kSortOrderKey, std::string(sort_order_to_string(valid.sort_order)));
    key_file->set_integer(kSettingsGroup, kWindowWidthKey, valid.window_width);
    key_file->set_integer(kSettingsGroup, kWindowHeightKey, valid.window_height);
    key_file->set_boolean(kSettingsGroup, kWindowMaximizedKey, valid.window_maximized);

    try {
        if (!key_file->save_to_file(file_path)) {
            return std::unexpected(Error{std::format("could not write preferences to {}", file_path)});
        }
    } catch (const Glib::Error& error) {
        return std::unexpected(Error{
            std::format("could not write preferences to {}: {}", file_path, error.what())});
    }

    return {};
}

std::expected<Preferences, Error> load_preferences() {
    return load_preferences_from_file(default_preferences_file_path());
}

std::expected<void, Error> save_preferences(const Preferences& preferences) {
    return save_preferences_to_file(default_preferences_file_path(), preferences);
}

} // namespace imagemgr
