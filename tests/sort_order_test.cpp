// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <array>

#include "core/sort_order.h"

using namespace imagemgr;

TEST_CASE("sort order: every value round-trips through its identifier", "[sort_order]") {
    constexpr std::array<SortOrder, 4> orders{
        SortOrder::NameAscending,
        SortOrder::NameDescending,
        SortOrder::ModifiedAscending,
        SortOrder::ModifiedDescending,
    };

    for (const SortOrder order : orders) {
        const auto parsed = sort_order_from_string(sort_order_to_string(order));
        REQUIRE(parsed.has_value());
        REQUIRE(*parsed == order);
    }
}

TEST_CASE("sort order: an unknown identifier has no value", "[sort_order]") {
    REQUIRE_FALSE(sort_order_from_string("banana").has_value());
    REQUIRE_FALSE(sort_order_from_string("").has_value());
}
