// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <giomm/asyncresult.h>
#include <giomm/file.h>
#include <giomm/fileenumerator.h>
#include <giomm/filemonitor.h>
#include <giomm/liststore.h>
#include <gtkmm/box.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/gridview.h>
#include <gtkmm/listitemfactory.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/stringlist.h>
#include <sigc++/sigc++.h>

#include "app/directory_item.h"
#include "core/preferences.h"
#include "core/sort_order.h"
#include "io/thumbnail_service.h"

namespace imagemgr {

/// Left pane: the directory toolbar above a thumbnail grid of the images in a
/// directory.
///
/// Directory enumeration runs asynchronously via Gio (never blocking the main
/// loop); thumbnails are generated on a worker pool by ThumbnailService and
/// applied back on the main thread. A Gio::FileMonitor watches the current
/// directory and triggers a debounced reload on changes.
///
/// Selection changes are reported through signal_image_selected(); decoding and
/// display of the chosen image belong to the image pane.
///
/// Thread-safety: main thread only (a Gtk::Widget).
class DirectoryPane : public Gtk::Box {
public:
    /// @param preferences Process-lifetime preferences owned by the
    ///        Application. Must outlive this pane. Thumbnail size and filename
    ///        visibility are read from it.
    explicit DirectoryPane(Preferences& preferences);
    ~DirectoryPane() override;

    /// Starts showing the image files in directory_path. Any previous scan is
    /// cancelled. Safe to call with an unreadable path (the grid simply stays
    /// empty and a warning is logged).
    void load_directory(const std::string& directory_path);

    /// Re-reads the settings that affect the grid (thumbnail size, filename
    /// visibility) and rebuilds the grid if they changed.
    void apply_preferences();

    /// Emitted on the main thread when the selected image changes. The argument
    /// is the selected file path, or empty when nothing is selected.
    sigc::signal<void(const std::string& path)>& signal_image_selected();

    /// Emitted when the user changes the sort order, so the owner can persist
    /// it. The grid is reordered before the signal fires.
    sigc::signal<void(SortOrder)>& signal_sort_order_changed();

private:
    void build_ui();
    Glib::RefPtr<Gtk::ListItemFactory> create_cell_factory();

    // Asynchronous directory enumeration (Gio). generation guards against
    // callbacks from a scan that has since been superseded.
    void on_children_enumerated(const Glib::RefPtr<Gio::AsyncResult>& result, guint generation);
    void request_next_scan_batch(guint generation);
    void on_scan_batch(const Glib::RefPtr<Gio::AsyncResult>& result, guint generation);
    void on_scan_finished(guint generation);

    void schedule_reload();
    void on_selection_changed();
    void on_thumbnail_ready(const std::string& path, int size,
                            Glib::RefPtr<Gdk::Pixbuf> thumbnail);

    /// Reorders the model using the current sort order.
    void resort_list_store();

    /// Applies the sort dropdown's current selection.
    void on_sort_order_selected();

    Preferences& preferences_;
    int thumbnail_size_ = 128;
    bool show_filenames_ = true;
    SortOrder sort_order_ = SortOrder::NameAscending;

    std::string current_directory_;
    Glib::RefPtr<Gio::File> current_directory_file_;
    Glib::RefPtr<Gio::FileEnumerator> current_enumerator_;
    Glib::RefPtr<Gio::FileMonitor> current_monitor_;
    sigc::connection reload_connection_;
    guint scan_generation_ = 0;

    Glib::RefPtr<Gio::ListStore<DirectoryItem>> list_store_;
    Glib::RefPtr<Gtk::SingleSelection> selection_model_;
    std::unordered_map<std::string, Glib::RefPtr<DirectoryItem>> item_by_path_;
    std::unique_ptr<ThumbnailService> thumbnail_service_;

    Gtk::ScrolledWindow scrolled_window_;
    Gtk::GridView grid_view_;
    Gtk::DropDown sort_dropdown_;
    sigc::signal<void(const std::string&)> image_selected_signal_;
    sigc::signal<void(SortOrder)> sort_order_changed_signal_;
};

} // namespace imagemgr
