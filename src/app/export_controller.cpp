// SPDX-License-Identifier: GPL-3.0-only

#include "app/export_controller.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>
#include <utility>

#include <giomm/file.h>
#include <giomm/fileinfo.h>
#include <glib.h>
#include <glibmm/miscutils.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/filedialog.h>

#include "io/export_image.h"
#include "io/preferences_store.h"

namespace imagemgr {

namespace {

// A fresh stable identity for an export record, used to address it in future
// re-export operations.
std::string new_record_id() {
    gchar* uuid = g_uuid_string_random();
    std::string result = uuid != nullptr ? uuid : std::string{};
    g_free(uuid);
    return result;
}

// The source file's modification time in Unix seconds, or 0 when unavailable.
std::int64_t file_modified_unix_seconds(const std::string& path) {
    try {
        const auto info = Gio::File::create_for_path(path)->query_info(
            "time::modified", Gio::FileQueryInfoFlags::NONE);
        if (!info) {
            return 0;
        }
        return static_cast<std::int64_t>(info->get_attribute_uint64("time::modified"));
    } catch (const Glib::Error&) {
        return 0;
    }
}

} // namespace

ExportController::ExportController(Preferences& preferences, Gtk::Window& parent,
                                   ExportHistoryStore& history_store)
    : preferences_(preferences),
      parent_(parent),
      history_store_(history_store) {}

void ExportController::export_selection(const ImageDocument& document) {
    if (!document.has_image()) {
        report_error("There is no image loaded to export.");
        return;
    }
    if (!document.has_selection()) {
        report_error("Select a region to export first.");
        return;
    }

    PendingExport pending;
    pending.source_path = document.source_path();
    pending.display_pixbuf = document.display_pixbuf();
    pending.crop_region = document.selection();
    // History records the region in the file's pixel space; a transient
    // view-only rotation (for formats that cannot be written) is undone here.
    pending.source_region = document.display_region_to_source(document.selection());
    pending.source_modified_unix_seconds = file_modified_unix_seconds(pending.source_path);
    pending.format = format_for_source(pending.source_path);
    pending.quality = preferences_.default_export_quality;

    if (!preferences_.last_export_directory.empty()) {
        perform_export(std::move(pending), preferences_.last_export_directory);
        return;
    }
    choose_directory([this, pending = std::move(pending)](const std::string& directory) {
        perform_export(pending, directory);
    });
}

void ExportController::prompt_for_export_directory() {
    choose_directory([this](const std::string& directory) {
        remember_export_directory(directory);
    });
}

void ExportController::choose_directory(sigc::slot<void(const std::string& directory)> on_chosen) {
    // Supersede any chooser that is still open so only the latest request can
    // act (and so its callback is not invoked twice).
    if (folder_cancellable_) {
        folder_cancellable_->cancel();
    }
    folder_cancellable_ = Gio::Cancellable::create();

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Choose export directory");
    if (!preferences_.last_export_directory.empty()) {
        dialog->set_initial_folder(
            Gio::File::create_for_path(preferences_.last_export_directory));
    }

    // The dialog is captured in its own callback so it outlives this call. The
    // callback body is wrapped so that no exception can escape into GTK (where
    // it would terminate the process).
    dialog->select_folder(
        parent_,
        [this, dialog, on_chosen = std::move(on_chosen)](
            const Glib::RefPtr<Gio::AsyncResult>& result) mutable {
            Glib::RefPtr<Gio::File> folder;
            try {
                folder = dialog->select_folder_finish(result);
            } catch (const Glib::Error&) {
                return; // The chooser was dismissed or cancelled.
            }
            if (!folder) {
                return;
            }
            const std::string directory = folder->get_path();
            if (directory.empty()) {
                return; // A non-local (portal) selection has no filesystem path.
            }
            try {
                on_chosen(directory);
            } catch (const std::exception& error) {
                report_error(error.what());
            }
        },
        folder_cancellable_);
}

void ExportController::perform_export(PendingExport pending, const std::string& directory) {
    ExportRequest request;
    request.target_directory = directory;
    request.crop_region = pending.crop_region;
    request.format = pending.format;
    request.quality = pending.quality;

    const auto result = export_region(pending.display_pixbuf, request);
    if (!result) {
        report_error(result.error().message);
        return;
    }

    remember_export_directory(directory);

    ExportRecord record;
    record.id = new_record_id();
    record.region = pending.source_region;
    record.output_path = *result;
    record.format = pending.format;
    record.exported_unix_seconds = g_get_real_time() / G_USEC_PER_SEC;

    const auto stored = history_store_.append(pending.source_path,
                                              pending.source_modified_unix_seconds, record);
    if (!stored) {
        // History is best-effort: the file was written, so still report success.
        g_warning("could not record export history: %s", stored.error().message.c_str());
    }

    export_succeeded_signal_.emit(pending.source_path, record);
}

void ExportController::remember_export_directory(const std::string& directory) {
    preferences_.last_export_directory = directory;
    const auto saved = save_preferences(preferences_);
    if (!saved) {
        g_warning("could not save preferences: %s", saved.error().message.c_str());
    }
    export_directory_changed_signal_.emit(directory);
}

void ExportController::report_error(const std::string& message) const {
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message("Export failed");
    dialog->set_detail(message);
    dialog->show(parent_);
}

sigc::signal<void(const std::string&, const ExportRecord&)>&
ExportController::signal_export_succeeded() {
    return export_succeeded_signal_;
}

sigc::signal<void(const std::string&)>& ExportController::signal_export_directory_changed() {
    return export_directory_changed_signal_;
}

ImageFormat ExportController::format_for_source(const std::string& source_path) {
    const std::size_t dot = source_path.find_last_of('.');
    if (dot == std::string::npos) {
        return ImageFormat::Png;
    }
    return format_from_extension(std::string_view(source_path).substr(dot + 1))
        .value_or(ImageFormat::Png);
}

} // namespace imagemgr
