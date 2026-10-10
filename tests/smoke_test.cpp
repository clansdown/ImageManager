// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <string>

// Verifies that the build is actually compiling as C++23 and that the Catch2
// test harness is wired up. Real core/ and io/ tests arrive with their
// respective milestones.
TEST_CASE("toolchain provides C++23 std::expected", "[smoke]") {
    const std::expected<int, std::string> success{42};
    REQUIRE(success.has_value());
    REQUIRE(*success == 42);

    const std::expected<int, std::string> failure{std::unexpect, "boom"};
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(failure.error() == "boom");
}
