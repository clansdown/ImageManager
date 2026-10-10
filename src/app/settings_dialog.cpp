// SPDX-License-Identifier: GPL-3.0-only

#include "app/settings_dialog.h"

#include <gtkmm/grid.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>

namespace imagemgr {

SettingsDialog::SettingsDialog(Gtk::Window& parent) {
    set_title("Settings");
    set_transient_for(parent);
    set_modal(true);
    set_hide_on_close(true);
    set_default_size(360, -1);

    auto* header_bar = Gtk::make_managed<Gtk::HeaderBar>();
    header_bar->set_show_title_buttons(false);

    cancel_button_.set_label("Cancel");
    save_button_.set_label("Save");
    save_button_.add_css_class("suggested-action");
    header_bar->pack_start(cancel_button_);
    header_bar->pack_end(save_button_);
    set_titlebar(*header_bar);

    auto* grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(6);
    grid->set_column_spacing(12);
    grid->set_margin(12);

    auto* quality_label = Gtk::make_managed<Gtk::Label>("Export quality");
    quality_label->set_xalign(0.0F);
    auto* thumbnail_label = Gtk::make_managed<Gtk::Label>("Thumbnail size (px)");
    thumbnail_label->set_xalign(0.0F);
    show_filenames_check_button_.set_label("Show filenames");
    show_filenames_check_button_.set_active(true);

    quality_spin_button_.set_range(kMinimumExportQuality, kMaximumExportQuality);
    quality_spin_button_.set_increments(1.0, 10.0);
    quality_spin_button_.set_numeric(true);
    thumbnail_size_spin_button_.set_range(kMinimumThumbnailSize, kMaximumThumbnailSize);
    thumbnail_size_spin_button_.set_increments(8.0, 32.0);
    thumbnail_size_spin_button_.set_numeric(true);

    grid->attach(*quality_label, 0, 0, 1, 1);
    grid->attach(quality_spin_button_, 1, 0, 1, 1);
    grid->attach(*thumbnail_label, 0, 1, 1, 1);
    grid->attach(thumbnail_size_spin_button_, 1, 1, 1, 1);
    grid->attach(show_filenames_check_button_, 0, 2, 2, 1);

    set_child(*grid);

    cancel_button_.signal_clicked().connect([this] { hide(); });
    save_button_.signal_clicked().connect([this] { on_save_clicked(); });
}

sigc::signal<void(const Preferences&)>& SettingsDialog::signal_preferences_saved() {
    return preferences_saved_signal_;
}

void SettingsDialog::set_preferences(const Preferences& preferences) {
    preferences_ = preferences;
    quality_spin_button_.set_value(preferences_.default_export_quality);
    thumbnail_size_spin_button_.set_value(preferences_.thumbnail_size);
    show_filenames_check_button_.set_active(preferences_.show_filenames_in_list);
}

void SettingsDialog::on_save_clicked() {
    preferences_.default_export_quality = quality_spin_button_.get_value_as_int();
    preferences_.thumbnail_size = thumbnail_size_spin_button_.get_value_as_int();
    preferences_.show_filenames_in_list = show_filenames_check_button_.get_active();
    preferences_saved_signal_.emit(preferences_);
    hide();
}

} // namespace imagemgr
