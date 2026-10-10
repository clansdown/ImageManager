// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <string>

#include "core/error.h"
#include "core/preferences.h"

namespace imagemgr {

/// Returns the path of the user's settings file:
/// $XDG_CONFIG_HOME/ImageManager/settings.ini (usually
/// ~/.config/ImageManager/settings.ini). The file need not exist.
[[nodiscard]] std::string default_preferences_file_path();

/// Loads preferences from an explicit file path.
///
/// A missing file is not an error: the built-in defaults are returned. Values
/// present in the file override the defaults, and the result is clamped into
/// valid ranges. Returns an Error only if the file exists but cannot be read or
/// parsed.
///
/// Thread-safety: pure function over the filesystem; safe to call off the main
/// thread.
[[nodiscard]] std::expected<Preferences, Error> load_preferences_from_file(
    const std::string& file_path);

/// Writes preferences to an explicit file path, creating parent directories as
/// needed. The value is clamped into valid ranges before writing.
///
/// Thread-safety: performs blocking file I/O; call off the main thread if the
/// delay matters.
[[nodiscard]] std::expected<void, Error> save_preferences_to_file(
    const std::string& file_path, const Preferences& preferences);

/// Convenience wrappers around the default settings file path.
[[nodiscard]] std::expected<Preferences, Error> load_preferences();
[[nodiscard]] std::expected<void, Error> save_preferences(const Preferences& preferences);

} // namespace imagemgr
