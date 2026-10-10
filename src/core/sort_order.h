// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>
#include <string_view>

namespace imagemgr {

/// How the directory grid is ordered.
///
/// The declaration order is not persisted; sort_order_to_string() provides the
/// stable identifiers so a settings file survives a reordering of this enum.
enum class SortOrder {
    NameAscending,
    NameDescending,
    ModifiedAscending,
    ModifiedDescending,
};

/// Stable identifier for a sort order, as written to the settings file.
[[nodiscard]] constexpr std::string_view sort_order_to_string(SortOrder order) {
    switch (order) {
    case SortOrder::NameAscending:
        return "name-ascending";
    case SortOrder::NameDescending:
        return "name-descending";
    case SortOrder::ModifiedAscending:
        return "modified-ascending";
    case SortOrder::ModifiedDescending:
        return "modified-descending";
    }
    return "name-ascending";
}

/// Parses an identifier produced by sort_order_to_string(). Returns nullopt for
/// an unknown value so callers can fall back to a default.
[[nodiscard]] inline std::optional<SortOrder> sort_order_from_string(std::string_view value) {
    if (value == "name-ascending") {
        return SortOrder::NameAscending;
    }
    if (value == "name-descending") {
        return SortOrder::NameDescending;
    }
    if (value == "modified-ascending") {
        return SortOrder::ModifiedAscending;
    }
    if (value == "modified-descending") {
        return SortOrder::ModifiedDescending;
    }
    return std::nullopt;
}

} // namespace imagemgr
