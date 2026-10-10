// SPDX-License-Identifier: GPL-3.0-only

#include "app/application.h"

#include "app/main_window.h"
#include "io/command_line_options.h"
#include "io/preferences_store.h"

#include <gdkmm/display.h>
#include <glib.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/styleprovider.h>

namespace imagemgr {

namespace {

// The GTK theme puts a 12px margin and an 8px border-spacing on boxes inside a
// gridview cell, which wastes a lot of space around each thumbnail. Override
// both for our cells. The theme's 3px child padding is deliberately left as the
// inter-cell gap. The export badge floats over the thumbnail's top-right corner.
constexpr auto kApplicationCss = R"(
.thumbnail-cell {
    margin: 0;
    border-spacing: 0;
}

.export-badge {
    background-color: rgba(0, 0, 0, 0.65);
    color: #ffffff;
    font-size: 0.75em;
    font-weight: bold;
    padding: 1px 5px;
    border: 1px solid #33e633;
    border-radius: 9px;
    box-shadow: 0 0 3px rgba(51, 230, 51, 0.85);
    margin: 4px;
}
)";

// Loads kApplicationCss at application priority, above the theme.
void install_application_css() {
    const auto display = Gdk::Display::get_default();
    if (!display) {
        g_warning("no display; skipping application CSS");
        return;
    }
    auto provider = Gtk::CssProvider::create();
    provider->load_from_string(kApplicationCss);
    Gtk::StyleProvider::add_provider_for_display(display, provider,
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

} // namespace

Application::Application()
    : Gtk::Application("org.imagemanager.ImageManager", Gio::Application::Flags::NON_UNIQUE) {
    register_command_line_options();
}

Glib::RefPtr<Application> Application::create() {
    return Glib::RefPtr<Application>(new Application());
}

void Application::register_command_line_options() {
    add_main_option_entry(Gio::Application::OptionType::STRING, "directory", 'd',
                          "Directory to show at startup", "DIR");
    add_main_option_entry(Gio::Application::OptionType::STRING, "export-to", 'o',
                          "Directory to write exports to", "DIR");
    add_main_option_entry(Gio::Application::OptionType::INT, "quality", 'q',
                          "Export quality, 0-100", "N");
    // Positional arguments are collected under G_OPTION_REMAINING and handled by
    // command_line_options_from_dict().
    add_main_option_entry(Gio::Application::OptionType::STRING_VECTOR, G_OPTION_REMAINING, '\0',
                          "Directory to show at startup", "DIR");
}

int Application::on_handle_local_options(const Glib::RefPtr<Glib::VariantDict>& options) {
    if (options) {
        command_line_options_ = command_line_options_from_dict(*options);
    }
    return -1; // Continue and activate the application.
}

void Application::on_startup() {
    Gtk::Application::on_startup();

    install_application_css();

    // Preferences are loaded once and then shared with the windows. A missing
    // file yields defaults; only a genuinely broken file is reported.
    const auto loaded = load_preferences();
    if (loaded) {
        preferences_ = *loaded;
    } else {
        g_warning("could not load preferences, using defaults: %s", loaded.error().message.c_str());
    }

    // Command-line overrides take precedence over the saved preferences. They
    // are folded in here so the rest of the application only ever consults
    // preferences_.
    if (command_line_options_.export_directory) {
        preferences_.last_export_directory = *command_line_options_.export_directory;
    }
    if (command_line_options_.export_quality) {
        preferences_.default_export_quality = *command_line_options_.export_quality;
    }

    // Application-scoped actions and accelerators are registered here so they
    // exist before the first window is created.
}

void Application::on_activate() {
    Gtk::Application::on_activate();

    // add_window() takes ownership: the Application deletes the window when it
    // is removed or the application exits. MainWindow is intentionally
    // heap-allocated because of this framework contract.
    auto* window = new MainWindow(preferences_, command_line_options_);
    add_window(*window);
    window->present();
}

} // namespace imagemgr
