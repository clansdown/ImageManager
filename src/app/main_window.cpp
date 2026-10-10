// SPDX-License-Identifier: GPL-3.0-only

#include "app/main_window.h"

#include <gdk/gdkkeysyms.h>
#include <giomm/file.h>
#include <glib.h>
#include <glibmm/miscutils.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/headerbar.h>

#include "io/preferences_store.h"

namespace imagemgr {

namespace {

// Default window size used before the user has resized the window.
constexpr int kDefaultWindowWidth = 1200;
constexpr int kDefaultWindowHeight = 800;

} // namespace

MainWindow::MainWindow(Preferences& preferences, const CommandLineOptions& command_line_options)
    : preferences_(preferences),
      directory_pane_(preferences) {
    set_title("ImageManager");

    if (preferences_.window_width > 0 && preferences_.window_height > 0) {
        set_default_size(preferences_.window_width, preferences_.window_height);
    } else {
        set_default_size(kDefaultWindowWidth, kDefaultWindowHeight);
    }

    auto* header_bar = Gtk::make_managed<Gtk::HeaderBar>();

    auto* settings_button = Gtk::make_managed<Gtk::Button>();
    settings_button->set_icon_name("emblem-system-symbolic");
    settings_button->set_tooltip_text("Settings");
    settings_button->signal_clicked().connect([this] { on_settings_activated(); });
    header_bar->pack_end(*settings_button);

    set_titlebar(*header_bar);

    paned_.set_start_child(directory_pane_);
    paned_.set_end_child(image_pane_);

    // Keep the sidebar at a fixed width and let the image pane grow.
    paned_.set_resize_start_child(false);
    paned_.set_shrink_start_child(false);

    // The toast floats above the panes.
    overlay_.set_child(paned_);
    overlay_.add_overlay(toast_);
    set_child(overlay_);

    apply_preferences();

    directory_pane_.signal_image_selected().connect([this](const std::string& path) {
        image_pane_.set_image_name(path.empty() ? std::string{} : Glib::path_get_basename(path));
        image_pane_.load_image(path);
    });

    export_controller_ = std::make_unique<ExportController>(preferences_, *this);
    image_pane_.signal_export_requested().connect(
        [this] { export_controller_->export_selection(image_pane_.document()); });
    image_pane_.signal_choose_export_directory().connect(
        [this] { export_controller_->prompt_for_export_directory(); });
    image_pane_.set_export_directory_hint(preferences_.last_export_directory);

    // A successful export is a transient overlay, not a blocking dialog.
    export_controller_->signal_export_succeeded().connect(
        [this](const std::string& file_path) {
            show_toast("Exported " + Glib::path_get_basename(file_path));
        });
    export_controller_->signal_export_directory_changed().connect(
        [this](const std::string& directory) {
            image_pane_.set_export_directory_hint(directory);
            show_toast("Exports go to " + directory);
        });

    // The sort order is owned by the directory pane; persist changes here.
    directory_pane_.signal_sort_order_changed().connect([this](SortOrder order) {
        preferences_.sort_order = order;
        const auto saved = save_preferences(preferences_);
        if (!saved) {
            g_warning("could not save preferences: %s", saved.error().message.c_str());
        }
    });

    if (command_line_options.initial_directory) {
        directory_pane_.load_directory(*command_line_options.initial_directory);
    }

    // Ctrl+, opens the settings dialog.
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    key_controller->signal_key_pressed().connect(
        [this](guint keyval, guint /*keycode*/, Gdk::ModifierType state) -> bool {
            const bool control_pressed =
                (state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK;
            if (keyval == GDK_KEY_comma && control_pressed) {
                on_settings_activated();
                return true;
            }
            // Ctrl+E exports the current selection.
            if (keyval == GDK_KEY_e && control_pressed) {
                export_controller_->export_selection(image_pane_.document());
                return true;
            }
            // Ctrl+O opens a directory; Ctrl+Q closes the window (and quits).
            if (keyval == GDK_KEY_o && control_pressed) {
                on_open_directory();
                return true;
            }
            if (keyval == GDK_KEY_q && control_pressed) {
                close();
                return true;
            }
            // [ and ] rotate the image, unless a command modifier is held.
            if (keyval == GDK_KEY_bracketleft && !control_pressed) {
                image_pane_.rotate_counter_clockwise();
                return true;
            }
            if (keyval == GDK_KEY_bracketright && !control_pressed) {
                image_pane_.rotate_clockwise();
                return true;
            }
            // S toggles the selection tool; Esc clears the selection.
            if (keyval == GDK_KEY_s && !control_pressed) {
                image_pane_.toggle_selection_tool();
                return true;
            }
            if (keyval == GDK_KEY_Escape) {
                image_pane_.clear_selection();
                return true;
            }
            return false;
        },
        false);
    add_controller(key_controller);

    signal_close_request().connect([this] { return on_close_request(); }, false);

    if (preferences_.window_maximized) {
        maximize();
    }
}

void MainWindow::on_settings_activated() {
    if (!settings_dialog_) {
        settings_dialog_ = std::make_unique<SettingsDialog>(*this);
        settings_dialog_->signal_preferences_saved().connect(
            [this](const Preferences& preferences) { on_preferences_saved(preferences); });
    }
    settings_dialog_->set_preferences(preferences_);
    settings_dialog_->present();
}

void MainWindow::on_open_directory() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Open directory");
    dialog->select_folder(*this,
                          [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
                              Glib::RefPtr<Gio::File> folder;
                              try {
                                  folder = dialog->select_folder_finish(result);
                              } catch (const Glib::Error&) {
                                  return; // The chooser was dismissed.
                              }
                              if (!folder) {
                                  return;
                              }
                              const std::string directory = folder->get_path();
                              if (!directory.empty()) {
                                  directory_pane_.load_directory(directory);
                              }
                          });
}

void MainWindow::on_preferences_saved(const Preferences& preferences) {
    preferences_ = clamped_to_valid_ranges(preferences);
    apply_preferences();

    const auto saved = save_preferences(preferences_);
    if (!saved) {
        g_warning("could not save preferences: %s", saved.error().message.c_str());
    }
}

void MainWindow::apply_preferences() {
    paned_.set_position(preferences_.sidebar_width);
    directory_pane_.apply_preferences();
}

void MainWindow::show_toast(const Glib::ustring& message) {
    toast_.show_message(message);
}

bool MainWindow::on_close_request() {
    preferences_.sidebar_width = paned_.get_position();
    preferences_.window_maximized = is_maximized();
    if (!is_maximized()) {
        preferences_.window_width = get_width();
        preferences_.window_height = get_height();
    }

    const auto saved = save_preferences(preferences_);
    if (!saved) {
        g_warning("could not save preferences: %s", saved.error().message.c_str());
    }

    return false; // Allow the window to close.
}

} // namespace imagemgr
