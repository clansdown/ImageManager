// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>
#include <string>

namespace imagemgr {

/// Startup inputs parsed from the command line, before any UI exists.
///
/// All fields are optional: absent means "no override", so the corresponding
/// saved preference or built-in default applies.
struct CommandLineOptions {
    /// Directory to show in the sidebar at startup.
    std::optional<std::string> initial_directory;

    /// Directory exports are written to, overriding Preferences::last_export_directory.
    std::optional<std::string> export_directory;

    /// Export quality (0-100), overriding Preferences::default_export_quality.
    std::optional<int> export_quality;
};

} // namespace imagemgr
