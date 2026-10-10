// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace imagemgr {

/// Returns directory_path with a leading home directory collapsed to "~", for
/// display in the window title.
///
/// A path equal to home_directory, or beginning with home_directory followed by
/// a path separator, becomes "~" or "~/...". Any other path is returned
/// unchanged, so a sibling such as /home/other is never rewritten and a path
/// outside the home directory keeps its full form. A single trailing separator
/// on home_directory is ignored, so "/home/user/" and "/home/user" behave
/// identically. When home_directory is empty (or only a separator) the path is
/// returned as is.
///
/// This is a pure string operation: it performs no filesystem access and no
/// canonicalisation, so the caller decides what "home" means. Thread-safety:
/// pure function, safe to call from any thread.
[[nodiscard]] inline std::string abbreviate_home_directory(std::string_view directory_path,
                                                           std::string_view home_directory) {
    if (home_directory.empty()) {
        return std::string(directory_path);
    }

    if (home_directory.ends_with('/')) {
        home_directory.remove_suffix(1);
        if (home_directory.empty()) {
            return std::string(directory_path);
        }
    }

    if (directory_path == home_directory) {
        return "~";
    }

    if (directory_path.size() > home_directory.size() &&
        directory_path.starts_with(home_directory) &&
        directory_path[home_directory.size()] == '/') {
        return "~" + std::string(directory_path.substr(home_directory.size()));
    }

    return std::string(directory_path);
}

} // namespace imagemgr
