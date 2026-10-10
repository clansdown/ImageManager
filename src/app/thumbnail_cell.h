// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>
#include <sigc++/sigc++.h>

#include "app/directory_item.h"

namespace imagemgr {

/// One cell of the directory grid: a thumbnail above an optional file name.
///
/// Created by the list-item factory during setup and updated during bind. The
/// same widget instance is reused for different items, so every field is set
/// explicitly on each bind.
///
/// The cell observes the item it is bound to (observe_item()) so that a
/// thumbnail arriving after the bind still repaints the cell; GTK does not
/// re-emit bind for an item whose pointer is unchanged.
///
/// Thread-safety: main thread only (a Gtk::Widget).
class ThumbnailCell : public Gtk::Box {
public:
    ThumbnailCell();

    /// Sets the square edge length reserved for the thumbnail.
    void set_thumbnail_size(int size);

    /// Shows the given thumbnail image.
    void set_thumbnail(const Glib::RefPtr<Gdk::Pixbuf>& thumbnail);

    /// Shows the placeholder used while no thumbnail is available or decoding
    /// failed.
    void show_missing_thumbnail();

    /// Sets the file name shown under the thumbnail.
    void set_display_name(const Glib::ustring& display_name);

    /// Shows or hides the file name (Preferences::show_filenames_in_list).
    void set_show_name(bool show);

    /// Shows the export-count badge; hides it when count is zero.
    void set_export_count(std::size_t count);

    /// Binds this cell to item and repaints from its current thumbnail. Drops
    /// any previous observation first. Call from the factory's bind handler.
    void observe_item(const Glib::RefPtr<DirectoryItem>& item);

    /// Stops observing the current item. Call from the factory's unbind handler.
    void clear_item();

private:
    /// Repaints the thumbnail (or placeholder) from the observed item.
    void refresh_thumbnail_from_item();

    /// Repaints the export-count badge from the observed item.
    void refresh_export_badge_from_item();

    Gtk::Overlay thumbnail_overlay_;
    Gtk::Image thumbnail_image_;
    Gtk::Label export_badge_;
    Gtk::Label name_label_;
    int thumbnail_size_ = 128;
    Glib::RefPtr<DirectoryItem> observed_item_;
    sigc::connection thumbnail_connection_;
    sigc::connection export_count_connection_;
};

} // namespace imagemgr
