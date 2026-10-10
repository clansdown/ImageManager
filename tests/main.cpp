// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_session.hpp>

#include <giomm/init.h>
#include <gtkmm/init.h>

// Catch2 test runner. These register the glibmm/giomm/gdkmm/gtkmm wrapper
// tables so that C objects (Gio::File, Gdk::Pixbuf, ...) can be wrapped; a
// Gtk::Application would normally do this, but the tests are headless. No
// display is needed.
int main(int argc, char** argv) {
    Gio::init();
    Gtk::init_gtkmm_internals();
    return Catch::Session().run(argc, argv);
}
