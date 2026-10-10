// SPDX-License-Identifier: GPL-3.0-only

#include "app/directory_pane.h"

#include <giomm/fileinfo.h>
#include <glib.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/label.h>
#include <gtkmm/signallistitemfactory.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "app/thumbnail_cell.h"
#include "io/directory_scanner.h"

namespace imagemgr {

namespace {

// Number of files fetched per asynchronous batch while enumerating a directory.
constexpr int kScanBatchSize = 200;

// Delay before reloading after a file-monitor event, to coalesce bursts.
constexpr guint kReloadDebounceMilliseconds = 250;

// Dropdown entries: this order is what the user sees, and the selected index
// maps back to the enum through this table.
struct SortOrderChoice {
    SortOrder order;
    const char* label;
};

constexpr std::array<SortOrderChoice, 4> kSortOrderChoices{{
    {SortOrder::NameAscending, "Name (A–Z)"},
    {SortOrder::NameDescending, "Name (Z–A)"},
    {SortOrder::ModifiedAscending, "Modified (oldest first)"},
    {SortOrder::ModifiedDescending, "Modified (newest first)"},
}};

int index_of_sort_order(SortOrder order) {
    for (std::size_t index = 0; index < kSortOrderChoices.size(); ++index) {
        if (kSortOrderChoices[index].order == order) {
            return static_cast<int>(index);
        }
    }
    return 0;
}

// Orders two items for the grid. Name sorts use the precomputed collation key;
// modified sorts tie-break on that key so the result is deterministic.
int compare_directory_items(const Glib::RefPtr<const DirectoryItem>& a,
                            const Glib::RefPtr<const DirectoryItem>& b, SortOrder order) {
    const int name_comparison = a->name_sort_key().compare(b->name_sort_key());
    int modified_comparison = 0;
    if (a->modified_unix_seconds() < b->modified_unix_seconds()) {
        modified_comparison = -1;
    } else if (a->modified_unix_seconds() > b->modified_unix_seconds()) {
        modified_comparison = 1;
    }

    switch (order) {
    case SortOrder::NameAscending:
        return name_comparison;
    case SortOrder::NameDescending:
        return -name_comparison;
    case SortOrder::ModifiedAscending:
        return modified_comparison != 0 ? modified_comparison : name_comparison;
    case SortOrder::ModifiedDescending:
        return modified_comparison != 0 ? -modified_comparison : name_comparison;
    }
    return 0;
}

} // namespace

DirectoryPane::DirectoryPane(Preferences& preferences)
    : preferences_(preferences),
      thumbnail_size_(preferences.thumbnail_size),
      show_filenames_(preferences.show_filenames_in_list),
      sort_order_(preferences.sort_order),
      thumbnail_service_(std::make_unique<ThumbnailService>(
          [this](const std::string& path, int size, Glib::RefPtr<Gdk::Pixbuf> thumbnail) {
              on_thumbnail_ready(path, size, std::move(thumbnail));
          })) {
    build_ui();
}

DirectoryPane::~DirectoryPane() = default;

void DirectoryPane::build_ui() {
    set_orientation(Gtk::Orientation::VERTICAL);

    auto* toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    toolbar->add_css_class("toolbar");

    auto* title = Gtk::make_managed<Gtk::Label>("Directory");
    title->set_margin(6);
    title->set_xalign(0.0F);
    title->set_hexpand(true);
    toolbar->append(*title);

    std::vector<Glib::ustring> sort_labels;
    sort_labels.reserve(kSortOrderChoices.size());
    for (const auto& choice : kSortOrderChoices) {
        sort_labels.emplace_back(choice.label);
    }
    sort_dropdown_.set_model(Gtk::StringList::create(sort_labels));
    sort_dropdown_.set_selected(static_cast<guint>(index_of_sort_order(sort_order_)));
    sort_dropdown_.set_tooltip_text("Sort order");
    sort_dropdown_.set_valign(Gtk::Align::CENTER);
    sort_dropdown_.property_selected().signal_changed().connect(
        [this] { on_sort_order_selected(); });
    toolbar->append(sort_dropdown_);

    list_store_ = Gio::ListStore<DirectoryItem>::create();
    selection_model_ = Gtk::SingleSelection::create(list_store_);

    grid_view_.set_model(selection_model_);
    grid_view_.set_factory(create_cell_factory());
    grid_view_.set_single_click_activate(false);

    scrolled_window_.set_child(grid_view_);
    scrolled_window_.set_vexpand(true);
    scrolled_window_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    append(*toolbar);
    append(scrolled_window_);

    selection_model_->signal_selection_changed().connect(
        [this](guint /*position*/, guint /*n_items*/) { on_selection_changed(); });
}

Glib::RefPtr<Gtk::ListItemFactory> DirectoryPane::create_cell_factory() {
    auto factory = Gtk::SignalListItemFactory::create();

    factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
        list_item->set_child(*Gtk::make_managed<ThumbnailCell>());
    });

    factory->signal_bind().connect([this](const Glib::RefPtr<Gtk::ListItem>& list_item) {
        auto* cell = dynamic_cast<ThumbnailCell*>(list_item->get_child());
        const auto item = std::dynamic_pointer_cast<DirectoryItem>(list_item->get_item());
        if (cell == nullptr || item == nullptr) {
            return;
        }

        cell->set_thumbnail_size(thumbnail_size_);
        cell->set_show_name(show_filenames_);
        cell->set_display_name(item->display_name());

        // Observe the item before requesting, so a synchronous cache-hit
        // delivery repaints this cell instead of relying on a rebind.
        cell->observe_item(item);
        if (!item->has_thumbnail()) {
            thumbnail_service_->request(item->path(), thumbnail_size_);
        }
    });

    factory->signal_unbind().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
        auto* cell = dynamic_cast<ThumbnailCell*>(list_item->get_child());
        if (cell != nullptr) {
            cell->clear_item();
        }
    });

    return factory;
}

void DirectoryPane::load_directory(const std::string& directory_path) {
    ++scan_generation_;
    const guint generation = scan_generation_;

    // Tear down anything belonging to the previous directory.
    reload_connection_.disconnect();
    current_monitor_.reset();
    current_enumerator_.reset();
    thumbnail_service_->cancel_all();
    list_store_->remove_all();
    item_by_path_.clear();

    current_directory_ = directory_path;
    current_directory_file_ = Gio::File::create_for_path(directory_path);

    current_directory_file_->enumerate_children_async(
        [this, generation](const Glib::RefPtr<Gio::AsyncResult>& result) {
            on_children_enumerated(result, generation);
        },
        "standard::name,standard::display-name,standard::type,standard::is-hidden,time::modified",
        Gio::FileQueryInfoFlags::NONE);
}

void DirectoryPane::on_children_enumerated(const Glib::RefPtr<Gio::AsyncResult>& result,
                                           guint generation) {
    if (generation != scan_generation_) {
        return;
    }
    try {
        current_enumerator_ = current_directory_file_->enumerate_children_finish(result);
    } catch (const Glib::Error& error) {
        g_warning("could not read directory %s: %s", current_directory_.c_str(), error.what());
        current_enumerator_.reset();
        return;
    }
    request_next_scan_batch(generation);
}

void DirectoryPane::request_next_scan_batch(guint generation) {
    if (generation != scan_generation_ || !current_enumerator_) {
        return;
    }
    current_enumerator_->next_files_async(
        [this, generation](const Glib::RefPtr<Gio::AsyncResult>& result) {
            on_scan_batch(result, generation);
        },
        kScanBatchSize);
}

void DirectoryPane::on_scan_batch(const Glib::RefPtr<Gio::AsyncResult>& result, guint generation) {
    if (generation != scan_generation_ || !current_enumerator_) {
        return;
    }

    std::vector<Glib::RefPtr<Gio::FileInfo>> file_infos;
    try {
        file_infos = current_enumerator_->next_files_finish(result);
    } catch (const Glib::Error& error) {
        g_warning("could not read directory %s: %s", current_directory_.c_str(), error.what());
        current_enumerator_.reset();
        return;
    }

    if (file_infos.empty()) {
        current_enumerator_->close();
        current_enumerator_.reset();
        on_scan_finished(generation);
        return;
    }

    for (const auto& info : file_infos) {
        if (info->get_file_type() != Gio::FileType::REGULAR || info->is_hidden()) {
            continue;
        }
        const std::string name = info->get_name();
        if (!is_supported_image_file_name(name)) {
            continue;
        }
        const std::string path = Glib::build_filename(current_directory_, name);
        const auto modified_seconds =
            static_cast<std::int64_t>(info->get_attribute_uint64("time::modified"));
        const auto item =
            DirectoryItem::create(path, info->get_display_name(), modified_seconds);
        list_store_->append(item);
        item_by_path_.emplace(path, item);
    }

    request_next_scan_batch(generation);
}

void DirectoryPane::on_scan_finished(guint generation) {
    if (generation != scan_generation_) {
        return;
    }

    // The scan appends in enumeration order; sort once everything is in.
    resort_list_store();

    current_monitor_ = current_directory_file_->monitor_directory(Gio::FileMonitorFlags::NONE);
    current_monitor_->signal_changed().connect(
        [this, generation](const Glib::RefPtr<Gio::File>& /*file*/,
                           const Glib::RefPtr<Gio::File>& /*other_file*/,
                           Gio::FileMonitor::Event event) {
            if (generation != scan_generation_) {
                return;
            }
            switch (event) {
            case Gio::FileMonitor::Event::CREATED:
            case Gio::FileMonitor::Event::DELETED:
            case Gio::FileMonitor::Event::MOVED_IN:
            case Gio::FileMonitor::Event::MOVED_OUT:
            case Gio::FileMonitor::Event::RENAMED:
                schedule_reload();
                break;
            default:
                break;
            }
        });
}

void DirectoryPane::schedule_reload() {
    if (reload_connection_.connected()) {
        return; // A reload is already pending.
    }
    reload_connection_ = Glib::signal_timeout().connect(
        [this] {
            if (!current_directory_.empty()) {
                load_directory(current_directory_);
            }
            return false; // One-shot.
        },
        kReloadDebounceMilliseconds);
}

void DirectoryPane::on_selection_changed() {
    const auto item =
        std::dynamic_pointer_cast<DirectoryItem>(selection_model_->get_selected_item());
    image_selected_signal_.emit(item ? item->path() : std::string{});
}

void DirectoryPane::on_thumbnail_ready(const std::string& path, int size,
                                       Glib::RefPtr<Gdk::Pixbuf> thumbnail) {
    if (size != thumbnail_size_) {
        return; // Requested at a stale size.
    }
    const auto found = item_by_path_.find(path);
    if (found == item_by_path_.end()) {
        return; // File no longer listed.
    }

    const auto& item = found->second;
    if (!item) {
        return;
    }
    // Notifies any bound cell, which repaints itself. GTK does not re-bind a
    // list item whose pointer is unchanged, so splicing the item back into the
    // store would not refresh the cell.
    item->set_thumbnail(std::move(thumbnail));
}

void DirectoryPane::resort_list_store() {
    list_store_->sort([this](const Glib::RefPtr<const DirectoryItem>& a,
                             const Glib::RefPtr<const DirectoryItem>& b) {
        return compare_directory_items(a, b, sort_order_);
    });
}

void DirectoryPane::on_sort_order_selected() {
    const guint selected = sort_dropdown_.get_selected();
    if (static_cast<std::size_t>(selected) >= kSortOrderChoices.size()) {
        return;
    }
    const SortOrder order = kSortOrderChoices[selected].order;
    if (order == sort_order_) {
        return; // Guards the programmatic set_selected() in apply_preferences().
    }
    sort_order_ = order;
    resort_list_store();
    sort_order_changed_signal_.emit(sort_order_);
}

void DirectoryPane::apply_preferences() {
    const int new_thumbnail_size = preferences_.thumbnail_size;
    const bool new_show_filenames = preferences_.show_filenames_in_list;
    const SortOrder new_sort_order = preferences_.sort_order;

    const bool grid_changed =
        new_thumbnail_size != thumbnail_size_ || new_show_filenames != show_filenames_;
    const bool sort_changed = new_sort_order != sort_order_;

    thumbnail_size_ = new_thumbnail_size;
    show_filenames_ = new_show_filenames;

    if (sort_changed) {
        sort_order_ = new_sort_order;
        sort_dropdown_.set_selected(static_cast<guint>(index_of_sort_order(sort_order_)));
        resort_list_store();
    }

    // Rebuild so existing cells pick up the new size/visibility. This resets
    // selection and scroll position, which is acceptable for a settings change.
    if (grid_changed && !current_directory_.empty()) {
        load_directory(current_directory_);
    }
}

sigc::signal<void(const std::string& path)>& DirectoryPane::signal_image_selected() {
    return image_selected_signal_;
}

sigc::signal<void(SortOrder)>& DirectoryPane::signal_sort_order_changed() {
    return sort_order_changed_signal_;
}

} // namespace imagemgr
