// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gtkmm/enums.h>
#include <gtkmm/label.h>
#include <gtkmm/revealer.h>
#include <sigc++/sigc++.h>

namespace imagemgr {

/// A transient, self-dismissing notification shown over the window content.
///
/// show_message() reveals the toast and hides it again after three seconds;
/// showing another message restarts the timer. It is meant to be added to a
/// Gtk::Overlay so it floats above the UI, and never intercepts input.
///
/// Thread-safety: main thread only (a Gtk::Widget).
class Toast : public Gtk::Revealer {
public:
    Toast();

    /// Shows message for three seconds, replacing any message already showing.
    void show_message(const Glib::ustring& message);

private:
    Gtk::Label label_;
    sigc::connection hide_connection_;
};

} // namespace imagemgr
