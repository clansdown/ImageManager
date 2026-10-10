// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace imagemgr {

/// A recoverable, user-presentable failure.
///
/// Core and io return std::expected<T, Error> for expected failures (a missing
/// file, malformed data, an unsupported format, a write that failed). Callers
/// log the message or surface it in the UI. Exceptions stay reserved for
/// programmer errors and for the gtkmm/GError boundary.
struct Error {
    std::string message;
};

} // namespace imagemgr
