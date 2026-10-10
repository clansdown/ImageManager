// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <glibmm/variantdict.h>

#include "core/command_line_options.h"

namespace imagemgr {

/// Extracts recognised command-line options from the dictionary produced by
/// Gio::Application's option parser.
///
/// @param options The dictionary delivered to
///        Gio::Application::on_handle_local_options(). Keys absent from the
///        dictionary leave the corresponding field empty. G_OPTION_REMAINING
///        (positional arguments), if present, supplies initial_directory when
///        --directory was not given.
/// @return The parsed options; never fails (unrecognised or malformed values
///         are simply ignored).
///
/// Thread-safety: pure function; safe to call from any thread.
[[nodiscard]] CommandLineOptions command_line_options_from_dict(const Glib::VariantDict& options);

} // namespace imagemgr
