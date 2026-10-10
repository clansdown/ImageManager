// SPDX-License-Identifier: GPL-3.0-only

#include "io/command_line_options.h"

#include <glib.h>

#include <string>
#include <vector>

namespace imagemgr {

CommandLineOptions command_line_options_from_dict(const Glib::VariantDict& options) {
    CommandLineOptions parsed;

    Glib::ustring directory;
    if (options.lookup_value("directory", directory)) {
        parsed.initial_directory = std::string(directory);
    }

    Glib::ustring export_directory;
    if (options.lookup_value("export-to", export_directory)) {
        parsed.export_directory = std::string(export_directory);
    }

    int quality = 0;
    if (options.lookup_value("quality", quality)) {
        parsed.export_quality = quality;
    }

    // Positional arguments are captured under G_OPTION_REMAINING. The first is
    // used as the startup directory when --directory was not supplied.
    if (!parsed.initial_directory) {
        std::vector<Glib::ustring> remaining;
        if (options.lookup_value(G_OPTION_REMAINING, remaining) && !remaining.empty()) {
            parsed.initial_directory = std::string(remaining.front());
        }
    }

    return parsed;
}

} // namespace imagemgr
