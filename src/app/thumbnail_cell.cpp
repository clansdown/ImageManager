// SPDX-License-Identifier: GPL-3.0-only

#include "app/thumbnail_cell.h"

#include <algorithm>

namespace imagemgr {

ThumbnailCell::ThumbnailCell()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 4) {
    set_halign(Gtk::Align::CENTER);

    // The app CSS provider clears the theme's 12px gridview-cell margin for
    // this class; the box spacing above is the whole thumbnail/label gap once
    // that provider also clears the theme's border-spacing.
    add_css_class("thumbnail-cell");

    thumbnail_image_.set_size_request(thumbnail_size_, thumbnail_size_);

    name_label_.set_justify(Gtk::Justification::CENTER);
    name_label_.set_ellipsize(Pango::EllipsizeMode::END);
    name_label_.set_max_width_chars(16);
    name_label_.set_wrap(true);
    name_label_.set_wrap_mode(Pango::WrapMode::WORD_CHAR);

    append(thumbnail_image_);
    append(name_label_);
}

void ThumbnailCell::set_thumbnail_size(int size) {
    thumbnail_size_ = size;
    thumbnail_image_.set_size_request(size, size);
    thumbnail_image_.set_pixel_size(size);
}

void ThumbnailCell::set_thumbnail(const Glib::RefPtr<Gdk::Pixbuf>& thumbnail) {
    // The placeholder path lowers pixel-size, and GTK caps a paintable's drawn
    // size at pixel-size (pixbufs included). Restore it, or the thumbnail
    // renders at half the configured size.
    thumbnail_image_.set_pixel_size(thumbnail_size_);
    thumbnail_image_.set(thumbnail);
}

void ThumbnailCell::show_missing_thumbnail() {
    thumbnail_image_.set_from_icon_name("image-missing-symbolic");
    thumbnail_image_.set_pixel_size(std::max(16, thumbnail_size_ / 2));
}

void ThumbnailCell::set_display_name(const Glib::ustring& display_name) {
    name_label_.set_text(display_name);
}

void ThumbnailCell::set_show_name(bool show) {
    name_label_.set_visible(show);
}

void ThumbnailCell::observe_item(const Glib::RefPtr<DirectoryItem>& item) {
    thumbnail_connection_.disconnect();
    observed_item_ = item;
    if (item) {
        thumbnail_connection_ =
            item->signal_thumbnail_changed().connect([this] { refresh_thumbnail_from_item(); });
    }
    refresh_thumbnail_from_item();
}

void ThumbnailCell::clear_item() {
    thumbnail_connection_.disconnect();
    observed_item_.reset();
}

void ThumbnailCell::refresh_thumbnail_from_item() {
    if (observed_item_ && observed_item_->has_thumbnail()) {
        set_thumbnail(observed_item_->thumbnail());
    } else {
        show_missing_thumbnail();
    }
}

} // namespace imagemgr
