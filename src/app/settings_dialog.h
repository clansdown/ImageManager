// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <sigc++/sigc++.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/window.h>

#include "core/preferences.h"

namespace imagemgr {

/// Modal settings dialog for editing user preferences.
///
/// Presents a small form (export quality, thumbnail size) seeded from a
/// Preferences value. On "Save" it emits signal_preferences_saved() with an
/// updated copy that preserves fields it does not edit; the owner is
/// responsible for persisting it. "Cancel" and closing the window discard the
/// edits.
///
/// Owned by MainWindow and reused across invocations. hide_on_close is enabled
/// so closing hides the dialog instead of destroying it.
///
/// Thread-safety: main thread only (a Gtk::Window).
class SettingsDialog : public Gtk::Window {
public:
    explicit SettingsDialog(Gtk::Window& parent);

    /// Emitted with the edited preferences when the user confirms with "Save".
    sigc::signal<void(const Preferences&)>& signal_preferences_saved();

    /// Seeds the form from the given preferences.
    void set_preferences(const Preferences& preferences);

private:
    void on_save_clicked();

    Preferences preferences_;
    Gtk::SpinButton quality_spin_button_;
    Gtk::SpinButton thumbnail_size_spin_button_;
    Gtk::CheckButton show_filenames_check_button_;
    Gtk::Button save_button_;
    Gtk::Button cancel_button_;
    sigc::signal<void(const Preferences&)> preferences_saved_signal_;
};

} // namespace imagemgr
