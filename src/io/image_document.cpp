// SPDX-License-Identifier: GPL-3.0-only

#include "io/image_document.h"

#include <utility>

namespace imagemgr {

void ImageDocument::set_source(std::string source_path,
                               Glib::RefPtr<Gdk::Pixbuf> source_pixbuf) {
    source_path_ = std::move(source_path);
    source_pixbuf_ = std::move(source_pixbuf);
    rotation_ = QuarterTurns::None;
    selection_ = ImageRectangle{};
    has_selection_ = false;
    rebuild_display_pixbuf();
}

void ImageDocument::clear() {
    source_path_.clear();
    source_pixbuf_.reset();
    display_pixbuf_.reset();
    rotation_ = QuarterTurns::None;
    selection_ = ImageRectangle{};
    has_selection_ = false;
}

ImageSize ImageDocument::display_size() const noexcept {
    if (display_pixbuf_ == nullptr) {
        return ImageSize{};
    }
    return ImageSize{.width = display_pixbuf_->get_width(),
                     .height = display_pixbuf_->get_height()};
}

void ImageDocument::rotate_clockwise() {
    if (!has_image()) {
        return;
    }
    const ImageSize before = display_size();
    rotation_ = turned_clockwise(rotation_);
    rebuild_display_pixbuf();
    if (has_selection_) {
        selection_ = rotate_rectangle_clockwise(selection_, before);
    }
}

void ImageDocument::rotate_counter_clockwise() {
    if (!has_image()) {
        return;
    }
    const ImageSize before = display_size();
    rotation_ = turned_counter_clockwise(rotation_);
    rebuild_display_pixbuf();
    if (has_selection_) {
        selection_ = rotate_rectangle_counter_clockwise(selection_, before);
    }
}

void ImageDocument::reset_rotation() {
    // Undo the accumulated quarter turns one step at a time; each step rotates
    // the selection correctly for the frame it is currently in.
    while (has_image() && rotation_ != QuarterTurns::None) {
        rotate_counter_clockwise();
    }
}

void ImageDocument::set_selection(ImageRectangle selection) {
    selection_ = clamp_rectangle(selection, display_size());
    has_selection_ = selection_.width > 0 && selection_.height > 0;
}

void ImageDocument::clear_selection() {
    selection_ = ImageRectangle{};
    has_selection_ = false;
}

void ImageDocument::rebuild_display_pixbuf() {
    if (source_pixbuf_ == nullptr) {
        display_pixbuf_.reset();
        return;
    }

    switch (rotation_) {
    case QuarterTurns::None:
        display_pixbuf_ = source_pixbuf_;
        break;
    case QuarterTurns::Clockwise:
        display_pixbuf_ = source_pixbuf_->rotate_simple(Gdk::Pixbuf::Rotation::CLOCKWISE);
        break;
    case QuarterTurns::Half:
        display_pixbuf_ = source_pixbuf_->rotate_simple(Gdk::Pixbuf::Rotation::UPSIDEDOWN);
        break;
    case QuarterTurns::CounterClockwise:
        display_pixbuf_ =
            source_pixbuf_->rotate_simple(Gdk::Pixbuf::Rotation::COUNTERCLOCKWISE);
        break;
    }
}

} // namespace imagemgr
