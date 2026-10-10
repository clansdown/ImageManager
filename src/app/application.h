// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <glibmm/refptr.h>
#include <glibmm/variantdict.h>
#include <gtkmm/application.h>

#include "core/command_line_options.h"
#include "core/preferences.h"

namespace imagemgr {

/// The Gtk::Application for ImageManager.
///
/// Owns application-wide concerns: command-line handling, actions and
/// accelerators, the loaded preferences, and creating the main window on
/// activation. Exactly one instance is created by main() through create().
///
/// The application is registered as NON_UNIQUE: every invocation is its own
/// primary instance rather than being routed into an already-running one. Each
/// process therefore parses its own command line and shows its own directory,
/// which is what allows several copies to run concurrently.
///
/// Thread-safety: Gtk::Application and its overrides run on the main thread
/// only.
class Application : public Gtk::Application {
public:
    /// Creates the application object.
    ///
    /// Gtk::Application's constructor is protected to force construction
    /// through a factory (mirroring Gtk::Application::create()), which also
    /// initialises GTK. Do not construct Application directly. The returned
    /// reference owns the sole Application instance.
    [[nodiscard]] static Glib::RefPtr<Application> create();

protected:
    Application();

    /// One-time setup on the primary instance, before activation. Loads
    /// preferences and registers application-scoped actions/accelerators.
    void on_startup() override;

    /// Creates and presents the main window the first time the application is
    /// activated; later activations reuse the existing window.
    void on_activate() override;

    /// Parses the local command line into command_line_options_. Returning a
    /// negative value tells GApplication to continue and activate normally.
    int on_handle_local_options(const Glib::RefPtr<Glib::VariantDict>& options) override;

public:
    /// The options parsed from this invocation's command line. Valid after the
    /// first on_handle_local_options() call (i.e. by the time a window exists).
    [[nodiscard]] const CommandLineOptions& command_line_options() const noexcept {
        return command_line_options_;
    }

private:
    // Registers the options shown in --help and accepted on the command line.
    // Must run before run() parses the arguments.
    void register_command_line_options();

    // Process-lifetime preferences shared with (and edited through) the main
    // window. Owned here; windows hold a reference to it.
    Preferences preferences_;

    // Parsed on this invocation, then folded into preferences_ during startup.
    CommandLineOptions command_line_options_;
};

} // namespace imagemgr
