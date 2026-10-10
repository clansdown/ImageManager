// SPDX-License-Identifier: GPL-3.0-only

#include "app/image_pane.h"

#include <cstdint>
#include <utility>

#include <giomm/file.h>
#include <giomm/fileinfo.h>
#include <glib.h>

#include "io/image_rotation.h"

namespace imagemgr {

namespace {
constexpr auto kNoImageText = "No image selected";
} // namespace

ImagePane::ImagePane(ExportHistoryStore& history_store)
    : Gtk::Box(Gtk::Orientation::VERTICAL),
      history_store_(history_store) {
    // Toolbar strip. Selection/export controls are added in M6-M7.
    auto* toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    toolbar->add_css_class("toolbar");

    rotate_counter_clockwise_button_.set_icon_name("object-rotate-left-symbolic");
    rotate_counter_clockwise_button_.set_tooltip_text("Rotate counter-clockwise ([)");
    rotate_counter_clockwise_button_.signal_clicked().connect(
        [this] { rotate_counter_clockwise(); });

    rotate_clockwise_button_.set_icon_name("object-rotate-right-symbolic");
    rotate_clockwise_button_.set_tooltip_text("Rotate clockwise (])");
    rotate_clockwise_button_.signal_clicked().connect([this] { rotate_clockwise(); });

    selection_button_.set_icon_name("edit-select-all-symbolic");
    selection_button_.set_tooltip_text("Selection tool (S)");
    selection_button_.signal_toggled().connect([this] {
        canvas_.set_selection_tool_active(selection_button_.get_active());
    });

    export_button_.set_icon_name("document-save-symbolic");
    export_button_.set_tooltip_text("Export selection (Ctrl+E)");
    export_button_.signal_clicked().connect([this] { export_requested_signal_.emit(); });

    choose_directory_button_.set_icon_name("folder-symbolic");
    choose_directory_button_.set_tooltip_text("Choose export directory");
    choose_directory_button_.signal_clicked().connect(
        [this] { choose_export_directory_signal_.emit(); });

    title_label_.set_text(kNoImageText);
    title_label_.set_margin(6);
    title_label_.set_xalign(0.0F);
    title_label_.set_hexpand(true);
    title_label_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);

    toolbar->append(rotate_counter_clockwise_button_);
    toolbar->append(rotate_clockwise_button_);
    toolbar->append(selection_button_);
    toolbar->append(export_button_);
    toolbar->append(choose_directory_button_);
    toolbar->append(title_label_);

    canvas_.set_document(&document_);
    canvas_.signal_selection_changed().connect([this] { update_export_sensitivity(); });
    scrolled_window_.set_child(canvas_);
    scrolled_window_.set_vexpand(true);
    scrolled_window_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    append(*toolbar);
    append(scrolled_window_);

    load_service_ = std::make_unique<ImageLoadService>(
        [this](const std::string& path,
               std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> result) {
            on_image_loaded(path, std::move(result));
        });

    rotation_save_service_ = std::make_unique<RotationSaveService>(
        [this](const std::string& path, QuarterTurns turns,
               std::expected<void, Error> result) {
            on_rotation_saved(path, turns, std::move(result));
        });

    update_tool_sensitivity();
}

ImagePane::~ImagePane() = default;

void ImagePane::set_image_name(const std::string& display_name) {
    title_label_.set_text(display_name.empty() ? kNoImageText : display_name);
}

void ImagePane::load_image(const std::string& path) {
    if (path.empty()) {
        load_service_->cancel();
        document_.clear();
        rotation_save_in_progress_ = false;
        canvas_.refresh();
        update_tool_sensitivity();
        return;
    }
    load_service_->load(path);
}

void ImagePane::rotate_clockwise() {
    request_rotation(true);
}

void ImagePane::rotate_counter_clockwise() {
    request_rotation(false);
}

void ImagePane::request_rotation(bool clockwise) {
    if (!document_.has_image() || rotation_save_in_progress_) {
        return;
    }
    const std::string path = document_.source_path();
    if (clockwise) {
        document_.rotate_clockwise();
    } else {
        document_.rotate_counter_clockwise();
    }
    canvas_.refresh();

    if (rotation_write_mode(path) == RotationWriteMode::ViewOnly) {
        return; // The view rotates; the file cannot be written losslessly.
    }

    rotation_save_in_progress_ = true;
    update_tool_sensitivity();
    rotation_save_service_->save(
        path, clockwise ? QuarterTurns::Clockwise : QuarterTurns::CounterClockwise,
        document_.display_pixbuf());
}

void ImagePane::on_rotation_saved(const std::string& path, QuarterTurns turns,
                                  std::expected<void, Error> result) {
    rotation_save_in_progress_ = false;
    update_tool_sensitivity();

    if (!document_.has_image() || document_.source_path() != path) {
        // The view moved on while saving; the file was still written.
        return;
    }
    if (!result) {
        // Revert the view rotation so it matches the unchanged file.
        if (turns == QuarterTurns::Clockwise) {
            document_.rotate_counter_clockwise();
        } else if (turns == QuarterTurns::CounterClockwise) {
            document_.rotate_clockwise();
        }
        canvas_.refresh();
        rotation_failed_signal_.emit(result.error().message);
        return;
    }

    document_.commit_rotation();
    persist_rotated_history(path);
    canvas_.refresh();
    rotation_written_signal_.emit(path);
}

void ImagePane::persist_rotated_history(const std::string& path) {
    // Nothing to rewrite when this image has never been exported.
    if (document_.exported_regions().empty() && !history_store_.contains(path)) {
        return;
    }

    ExportHistory history;
    history.source_path = path;
    history.records = document_.exported_regions();
    try {
        const auto info = Gio::File::create_for_path(path)->query_info(
            "time::modified", Gio::FileQueryInfoFlags::NONE);
        if (info) {
            history.source_modified_unix_seconds =
                static_cast<std::int64_t>(info->get_attribute_uint64("time::modified"));
        }
    } catch (const Glib::Error&) {
        // Leave the mtime at zero.
    }

    const auto saved = history_store_.save(history);
    if (!saved) {
        g_warning("could not update export history after rotation: %s",
                  saved.error().message.c_str());
    }
}

void ImagePane::toggle_selection_tool() {
    selection_button_.set_active(!selection_button_.get_active());
}

void ImagePane::clear_selection() {
    document_.clear_selection();
    canvas_.refresh();
    update_export_sensitivity();
}

sigc::signal<void()>& ImagePane::signal_export_requested() {
    return export_requested_signal_;
}

sigc::signal<void()>& ImagePane::signal_choose_export_directory() {
    return choose_export_directory_signal_;
}

sigc::signal<void(const std::string&)>& ImagePane::signal_rotation_written() {
    return rotation_written_signal_;
}

sigc::signal<void(const std::string&)>& ImagePane::signal_rotation_failed() {
    return rotation_failed_signal_;
}

void ImagePane::set_export_directory_hint(const std::string& directory) {
    if (directory.empty()) {
        choose_directory_button_.set_tooltip_text("Choose export directory");
    } else {
        choose_directory_button_.set_tooltip_text("Choose export directory (current: " +
                                                  directory + ")");
    }
}

void ImagePane::on_image_loaded(const std::string& path,
                                std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> result) {
    if (result) {
        document_.set_source(path, std::move(*result));
        // Overlay the regions exported from this image in earlier sessions.
        const auto history = history_store_.load(path);
        if (history && *history) {
            document_.set_exported_regions((*history)->records);
        }
    } else {
        g_warning("could not load image %s: %s", path.c_str(), result.error().message.c_str());
        document_.clear();
    }
    rotation_save_in_progress_ = false;
    canvas_.refresh();
    update_tool_sensitivity();
}

void ImagePane::note_exported_region(const std::string& source_path,
                                     const ExportRecord& record) {
    if (!document_.has_image() || document_.source_path() != source_path) {
        return;
    }
    document_.add_exported_region(record);
    canvas_.refresh();
}

void ImagePane::update_tool_sensitivity() {
    const bool has_image = document_.has_image();
    const bool can_rotate = has_image && !rotation_save_in_progress_;
    rotate_counter_clockwise_button_.set_sensitive(can_rotate);
    rotate_clockwise_button_.set_sensitive(can_rotate);
    selection_button_.set_sensitive(has_image);
    update_export_sensitivity();
}

void ImagePane::update_export_sensitivity() {
    export_button_.set_sensitive(document_.has_selection());
}

} // namespace imagemgr
