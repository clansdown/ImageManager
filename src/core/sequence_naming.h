// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace imagemgr {

namespace sequence_naming_detail {

inline bool is_all_digits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char character) {
               return character >= '0' && character <= '9';
           });
}

inline std::string zero_padded(long long value, std::size_t width) {
    std::string digits = std::to_string(value);
    if (digits.size() < width) {
        digits.insert(0, width - digits.size(), '0');
    }
    return digits;
}

} // namespace sequence_naming_detail

/// Chooses the next sequential export file name for a directory.
///
/// Every entry whose stem is entirely digits is considered, regardless of
/// extension, so the numbering is shared across formats in the directory. The
/// value one greater than the largest such stem is formatted with the same
/// number of digits as the widest of the maximal stems (e.g. `001`..`010` ->
/// `011`); when there are none the result is `1`. The extension is appended
/// after a dot.
///
/// As a safeguard the result is checked against the existing names and bumped
/// until it is unused (this should not normally trigger, since max+1 exceeds
/// every existing value).
[[nodiscard]] inline std::string next_sequential_filename(
    std::span<const std::string> directory_entry_names, std::string_view extension) {
    long long max_value = 0;
    std::size_t max_width = 0;
    bool found = false;

    for (const std::string& name : directory_entry_names) {
        const std::string_view view(name);
        const std::size_t dot = view.find_last_of('.');
        const std::string_view stem =
            dot == std::string_view::npos ? view : view.substr(0, dot);
        if (!sequence_naming_detail::is_all_digits(stem)) {
            continue;
        }
        if (stem.size() > 18) {
            continue; // Avoid overflow on absurdly long digit strings.
        }
        long long value = 0;
        for (const char digit : stem) {
            value = value * 10 + (digit - '0');
        }
        if (!found || value > max_value) {
            max_value = value;
            max_width = stem.size();
            found = true;
        }
    }

    long long next_value = found ? max_value + 1 : 1;
    const std::size_t width = found ? max_width : 1;

    const auto name_exists = [&](const std::string& candidate) {
        return std::find(directory_entry_names.begin(), directory_entry_names.end(),
                         candidate) != directory_entry_names.end();
    };

    std::string candidate =
        sequence_naming_detail::zero_padded(next_value, width) + "." + std::string(extension);
    while (name_exists(candidate)) {
        ++next_value;
        candidate =
            sequence_naming_detail::zero_padded(next_value, width) + "." + std::string(extension);
    }
    return candidate;
}

} // namespace imagemgr
