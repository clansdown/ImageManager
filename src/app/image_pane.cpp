// SPDX-License-Identifier: GPL-3.0-only

#include "app/image_pane.h"

#include <utility>

#include <glib.h>

namespace imagemgr {

namespace {
constexpr auto kNoImageText = "No image selected";
} // namespace

ImagePane::ImagePane()
    : Gtk::Box(Gtk::Orientation::VERTICAL) {
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
        canvas_.refresh();
        update_tool_sensitivity();
        return;
    }
    load_service_->load(path);
}

void ImagePane::rotate_clockwise() {
    if (!document_.has_image()) {
        return;
    }
    document_.rotate_clockwise();
    canvas_.refresh();
}

void ImagePane::rotate_counter_clockwise() {
    if (!document_.has_image()) {
        return;
    }
    document_.rotate_counter_clockwise();
    canvas_.refresh();
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
    } else {
        g_warning("could not load image %s: %s", path.c_str(), result.error().message.c_str());
        document_.clear();
    }
    canvas_.refresh();
    update_tool_sensitivity();
}

void ImagePane::update_tool_sensitivity() {
    const bool has_image = document_.has_image();
    rotate_counter_clockwise_button_.set_sensitive(has_image);
    rotate_clockwise_button_.set_sensitive(has_image);
    selection_button_.set_sensitive(has_image);
    update_export_sensitivity();
}

void ImagePane::update_export_sensitivity() {
    export_button_.set_sensitive(document_.has_selection());
}

} // namespace imagemgr
