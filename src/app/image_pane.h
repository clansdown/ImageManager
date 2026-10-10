// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <expected>
#include <memory>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <sigc++/sigc++.h>

#include "app/image_canvas.h"
#include "app/rotation_save_service.h"
#include "core/export_history.h"
#include "core/rotation.h"
#include "io/export_history_store.h"
#include "io/image_document.h"
#include "io/image_load_service.h"

namespace imagemgr {

/// Right pane: the image toolbar above the image view.
///
/// Owns the ImageDocument that backs the view and an ImageLoadService that
/// decodes the selected file off the main thread. Rotation, selection and
/// export controls are added in M5-M7; M4 establishes the pane structure, the
/// document and the fit-to-window canvas.
///
/// Thread-safety: main thread only (a Gtk::Widget).
class ImagePane : public Gtk::Box {
public:
    /// @param history_store Export history read when an image is opened and
    ///        appended to on a live export. Must outlive this pane.
    explicit ImagePane(ExportHistoryStore& history_store);
    ~ImagePane() override;

    /// Shows display_name in the toolbar, or a placeholder when it is empty.
    void set_image_name(const std::string& display_name);

    /// Loads and displays the image at path asynchronously. An empty path
    /// cancels any pending load and clears the view.
    void load_image(const std::string& path);

    /// Rotates the displayed image 90 degrees clockwise (selection included).
    /// Does nothing when no image is loaded.
    void rotate_clockwise();

    /// Rotates the displayed image 90 degrees counter-clockwise (selection
    /// included). Does nothing when no image is loaded.
    void rotate_counter_clockwise();

    /// Turns the region-selection tool on or off (bound to the S key and the
    /// toolbar toggle).
    void toggle_selection_tool();

    /// Discards the current selection (bound to Esc).
    void clear_selection();

    /// Emitted when the Export button is pressed. The owner runs the export,
    /// since it holds the preferences and the window for dialogs.
    sigc::signal<void()>& signal_export_requested();

    /// Emitted when the choose-export-directory button is pressed. The owner
    /// shows the folder chooser, since it holds the preferences.
    sigc::signal<void()>& signal_choose_export_directory();

    /// Updates the choose-directory button's tooltip to name the current export
    /// target. An empty directory resets it to the generic text.
    void set_export_directory_hint(const std::string& directory);

    /// Records a live export of the currently displayed image so its overlay
    /// updates immediately. Does nothing when a different image is displayed.
    void note_exported_region(const std::string& source_path, const ExportRecord& record);

    /// Emitted on the main thread after a rotation has been written to the
    /// file, with the file path, so the owner can refresh the grid entry.
    sigc::signal<void(const std::string& path)>& signal_rotation_written();

    /// Emitted on the main thread when writing a rotation failed, with a
    /// user-presentable message.
    sigc::signal<void(const std::string& message)>& signal_rotation_failed();

    /// The document backing the view; never null.
    [[nodiscard]] const ImageDocument& document() const noexcept { return document_; }

    /// Whether a region is currently selected.
    [[nodiscard]] bool has_selection() const noexcept { return document_.has_selection(); }

private:
    /// Receives the decoded image (or an error) from the load service, always
    /// on the main thread.
    void on_image_loaded(const std::string& path,
                         std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> result);

    /// Enables the image tools only while an image is loaded.
    void update_tool_sensitivity();

    /// Enables Export only while a region is selected.
    void update_export_sensitivity();

    /// Rotates the view one quarter turn and, for formats that can be written
    /// losslessly, persists the rotation asynchronously.
    void request_rotation(bool clockwise);

    /// Receives the result of an asynchronous rotation write.
    void on_rotation_saved(const std::string& path, QuarterTurns turns,
                           std::expected<void, Error> result);

    /// Rewrites the persisted export regions to match the committed rotation.
    void persist_rotated_history(const std::string& path);

    Gtk::Label title_label_;
    Gtk::Button rotate_counter_clockwise_button_;
    Gtk::Button rotate_clockwise_button_;
    Gtk::ToggleButton selection_button_;
    Gtk::Button export_button_;
    Gtk::Button choose_directory_button_;
    Gtk::ScrolledWindow scrolled_window_;
    ImageCanvas canvas_;
    ImageDocument document_;
    ExportHistoryStore& history_store_;
    std::unique_ptr<ImageLoadService> load_service_;
    std::unique_ptr<RotationSaveService> rotation_save_service_;
    bool rotation_save_in_progress_ = false;
    sigc::signal<void()> export_requested_signal_;
    sigc::signal<void()> choose_export_directory_signal_;
    sigc::signal<void(const std::string&)> rotation_written_signal_;
    sigc::signal<void(const std::string&)> rotation_failed_signal_;
};

} // namespace imagemgr
