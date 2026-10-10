// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <string>

#include <gdkmm/pixbuf.h>
#include <giomm/cancellable.h>
#include <glibmm/refptr.h>
#include <gtkmm/window.h>
#include <sigc++/sigc++.h>

#include "core/export.h"
#include "core/export_history.h"
#include "core/geometry.h"
#include "core/preferences.h"
#include "io/export_history_store.h"
#include "io/image_document.h"

namespace imagemgr {

/// Drives exports and the choice of export directory.
///
/// The target directory is Preferences::last_export_directory (which the
/// `--export-to` option folds into at startup); when it is empty, exporting
/// first shows a Gtk::FileDialog folder chooser. prompt_for_export_directory()
/// shows the same chooser even when a directory is already remembered, so the
/// user can change the target at any time. The format defaults to the loaded
/// file's format and the quality comes from preferences.
///
/// Errors are reported with a Gtk::AlertDialog. A successful export is reported
/// through signal_export_succeeded() so the window can show a transient
/// notification instead of a blocking dialog; choosing a directory is reported
/// through signal_export_directory_changed().
///
/// Thread-safety: main thread only (it shows dialogs).
class ExportController {
public:
    /// @param preferences Process-lifetime preferences owned by the
    ///        Application. Must outlive this controller.
    /// @param parent Window the dialogs are parented to. Must outlive this
    ///        controller.
    /// @param history_store Export history to append each successful export to.
    ///        Must outlive this controller.
    ExportController(Preferences& preferences, Gtk::Window& parent,
                     ExportHistoryStore& history_store);

    /// Exports document's current selection. Reports and returns quietly when
    /// there is nothing to export.
    void export_selection(const ImageDocument& document);

    /// Shows the folder chooser regardless of whether a directory is already
    /// remembered, and remembers the chosen directory. Bound to the toolbar
    /// button so the target can be changed before exporting.
    void prompt_for_export_directory();

    /// Emitted with the source image path and the record after a successful
    /// export, so the window can update the badge and the region overlay.
    sigc::signal<void(const std::string& source_path, const ExportRecord& record)>&
    signal_export_succeeded();

    /// Emitted with the chosen directory whenever the export target changes,
    /// whether through the export flow or prompt_for_export_directory().
    sigc::signal<void(const std::string& directory)>& signal_export_directory_changed();

private:
    // The image data captured when the export is requested, so that an
    // asynchronous folder chooser cannot race a selection change.
    struct PendingExport {
        std::string source_path;
        Glib::RefPtr<Gdk::Pixbuf> display_pixbuf;
        ImageRectangle crop_region;      // display (rotated) coordinates
        ImageRectangle source_region;    // source-image coordinates, for history
        std::int64_t source_modified_unix_seconds = 0;
        ImageFormat format = ImageFormat::Png;
        int quality = 90;
    };

    /// Shows the folder chooser and invokes on_chosen with the chosen directory.
    /// A previously pending chooser is cancelled first, so only the most recent
    /// request can complete.
    void choose_directory(sigc::slot<void(const std::string& directory)> on_chosen);

    void perform_export(PendingExport pending, const std::string& directory);

    /// Stores directory in preferences and persists it, then announces the
    /// change via signal_export_directory_changed().
    void remember_export_directory(const std::string& directory);

    void report_error(const std::string& message) const;

    /// The default format for a source path, based on its extension (PNG when
    /// the extension is missing or unrecognised).
    [[nodiscard]] static ImageFormat format_for_source(const std::string& source_path);

    Preferences& preferences_;
    Gtk::Window& parent_;
    ExportHistoryStore& history_store_;

    // Cancels a folder chooser that is still open when a new one is requested.
    Glib::RefPtr<Gio::Cancellable> folder_cancellable_;

    sigc::signal<void(const std::string&, const ExportRecord&)> export_succeeded_signal_;
    sigc::signal<void(const std::string&)> export_directory_changed_signal_;
};

} // namespace imagemgr
