// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>

#include <gtkmm/applicationwindow.h>
#include <gtkmm/overlay.h>
#include <gtkmm/paned.h>

#include "app/directory_pane.h"
#include "app/export_controller.h"
#include "app/image_pane.h"
#include "app/settings_dialog.h"
#include "app/toast.h"
#include "core/command_line_options.h"
#include "core/preferences.h"
#include "io/export_history_store.h"

namespace imagemgr {

/// The single top-level window.
///
/// Hosts the two-pane layout: a resizable Gtk::Paned whose start child is the
/// directory pane (directory toolbar + thumbnail grid) and whose end child is
/// the image pane (image toolbar + image canvas). The paned's gutter is the
/// draggable divider between them.
///
/// It edits the shared Preferences (sidebar width, window geometry, settings)
/// and persists them when closed.
///
/// The window is owned by the Application (see Application::on_activate); it
/// must be heap-allocated and must only be touched from the main thread.
class MainWindow : public Gtk::ApplicationWindow {
public:
    /// @param preferences Process-lifetime preferences owned by the
    ///        Application. Must outlive this window.
    /// @param command_line_options Options for this invocation; the initial
    ///        directory, if given, is loaded at startup.
    MainWindow(Preferences& preferences, const CommandLineOptions& command_line_options);

private:
    /// Presents (creating on first use) the settings dialog seeded with the
    /// current preferences.
    void on_settings_activated();

    /// Shows a folder chooser and loads the chosen directory in the directory
    /// pane.
    void on_open_directory();

    /// Updates the window title to name the directory the pane is showing,
    /// collapsing a leading home directory to "~". An empty path restores the
    /// plain application name.
    void on_directory_changed(const std::string& directory_path);

    /// Receives edited preferences from the settings dialog, stores them and
    /// persists them.
    void on_preferences_saved(const Preferences& preferences);

    /// Applies values from preferences_ that affect the live window (sidebar
    /// width, ...).
    void apply_preferences();

    /// Captures window geometry/sidebar width into preferences_ and writes them
    /// to disk. Always returns false so the close proceeds.
    bool on_close_request();

    /// Shows a transient overlay message over the window content.
    void show_toast(const Glib::ustring& message);

    Preferences& preferences_;

    // Persistent per-source export history, shared by the panes and the export
    // controller. Declared before them so it outlives their references.
    ExportHistoryStore export_history_store_;

    // Left pane stays its preferred width; the image pane absorbs any extra
    // space when the window is resized.
    Gtk::Paned paned_{Gtk::Orientation::HORIZONTAL};
    DirectoryPane directory_pane_;
    ImagePane image_pane_;

    // Created lazily on first open; hides (rather than destroys) on close.
    std::unique_ptr<SettingsDialog> settings_dialog_;

    // Runs exports; holds a reference back to this window for dialogs.
    std::unique_ptr<ExportController> export_controller_;

    // Floats a self-dismissing toast over the panes.
    Gtk::Overlay overlay_;
    Toast toast_;
};

} // namespace imagemgr
