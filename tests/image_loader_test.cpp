// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include "io/image_loader.h"

using namespace imagemgr;

TEST_CASE("image loader: a missing file is an error", "[image-loader]") {
    const auto loaded = load_image_scaled("/nonexistent/imagemgr-image.png", 64);

    REQUIRE_FALSE(loaded.has_value());
    REQUIRE_FALSE(loaded.error().message.empty());
}
