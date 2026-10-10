// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <gdkmm/pixbuf.h>

#include "io/image_document.h"

using namespace imagemgr;

namespace {

Glib::RefPtr<Gdk::Pixbuf> make_pixbuf(int width, int height) {
    return Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, width, height);
}

} // namespace

TEST_CASE("image document: starts empty", "[image-document]") {
    const ImageDocument document;

    REQUIRE_FALSE(document.has_image());
    REQUIRE(document.display_size() == ImageSize{});
}

TEST_CASE("image document: set_source installs the image unrotated", "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));

    REQUIRE(document.has_image());
    REQUIRE(document.source_path() == "/tmp/a.png");
    REQUIRE(document.rotation() == QuarterTurns::None);
    REQUIRE(document.display_size() == ImageSize{40, 20});
}

TEST_CASE("image document: rotation swaps the display size and turns the selection",
          "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));
    document.set_selection(ImageRectangle{5, 5, 10, 4});

    document.rotate_clockwise();

    REQUIRE(document.rotation() == QuarterTurns::Clockwise);
    REQUIRE(document.display_size() == ImageSize{20, 40});
    // before = {40, 20}: x = 20 - 5 - 4 = 11, y = 5.
    REQUIRE(document.has_selection());
    REQUIRE(document.selection() == ImageRectangle{11, 5, 4, 10});
}

TEST_CASE("image document: four clockwise rotations return to the original", "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));

    document.rotate_clockwise();
    document.rotate_clockwise();
    document.rotate_clockwise();
    document.rotate_clockwise();

    REQUIRE(document.rotation() == QuarterTurns::None);
    REQUIRE(document.display_size() == ImageSize{40, 20});
}

TEST_CASE("image document: reset_rotation returns to the source", "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));
    document.set_selection(ImageRectangle{5, 5, 10, 4});

    document.rotate_clockwise();
    document.reset_rotation();

    REQUIRE(document.rotation() == QuarterTurns::None);
    REQUIRE(document.display_size() == ImageSize{40, 20});
    REQUIRE(document.selection() == ImageRectangle{5, 5, 10, 4});
}

TEST_CASE("image document: the selection is clamped to the display", "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));

    document.set_selection(ImageRectangle{30, 15, 100, 100});

    REQUIRE(document.has_selection());
    REQUIRE(document.selection() == ImageRectangle{0, 0, 40, 20});
}

TEST_CASE("image document: clearing drops everything", "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));
    document.set_selection(ImageRectangle{1, 1, 2, 2});

    document.clear();

    REQUIRE_FALSE(document.has_image());
    REQUIRE_FALSE(document.has_selection());
    REQUIRE(document.source_path().empty());
}

TEST_CASE("image document: source and display regions map through the rotation",
          "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));

    const ImageRectangle source_region{5, 5, 10, 4};
    REQUIRE(document.source_size() == ImageSize{40, 20});
    REQUIRE(document.source_region_to_display(source_region) == source_region);

    document.rotate_clockwise();

    // before = {40, 20}: x = 20 - 5 - 4 = 11, y = 5; width/height swap.
    const ImageRectangle display_region{11, 5, 4, 10};
    REQUIRE(document.source_region_to_display(source_region) == display_region);
    REQUIRE(document.display_region_to_source(display_region) == source_region);
}

TEST_CASE("image document: commit_rotation bakes the view and rotates regions",
          "[image-document]") {
    ImageDocument document;
    document.set_source("/tmp/a.png", make_pixbuf(40, 20));

    ExportRecord record;
    record.region = ImageRectangle{5, 5, 10, 4};
    document.set_exported_regions({record});

    document.rotate_clockwise();
    document.commit_rotation();

    REQUIRE(document.rotation() == QuarterTurns::None);
    REQUIRE(document.source_size() == ImageSize{20, 40});
    REQUIRE(document.display_size() == ImageSize{20, 40});
    REQUIRE(document.exported_regions().size() == 1);
    REQUIRE(document.exported_regions()[0].region == ImageRectangle{11, 5, 4, 10});
}
