// SPDX-License-Identifier: GPL-3.0-only

#include "app/toast.h"

#include <glibmm/main.h>

namespace imagemgr {

namespace {

// How long a message stays on screen before it is hidden again.
constexpr guint kToastDurationSeconds = 3;

} // namespace

Toast::Toast() {
    set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
    set_halign(Gtk::Align::CENTER);
    set_valign(Gtk::Align::END);
    set_margin(12);
    add_css_class("osd");
    set_can_target(false); // Never intercept clicks meant for the UI below.

    label_.set_margin_top(6);
    label_.set_margin_bottom(6);
    label_.set_margin_start(12);
    label_.set_margin_end(12);
    set_child(label_);
}

void Toast::show_message(const Glib::ustring& message) {
    label_.set_text(message);
    set_reveal_child(true);

    hide_connection_.disconnect();
    hide_connection_ = Glib::signal_timeout().connect_seconds(
        [this] {
            set_reveal_child(false);
            return false; // One-shot.
        },
        kToastDurationSeconds);
}

} // namespace imagemgr
