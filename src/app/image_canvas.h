// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gdkmm/graphene_rect.h>
#include <gtkmm/widget.h>
#include <sigc++/sigc++.h>

#include "core/geometry.h"
#include "core/selection.h"

namespace imagemgr {

class ImageDocument;

/// Widget that draws the current image document and hosts the region-selection
/// tool.
///
/// The image is drawn fit to the widget, centered, and never enlarged past 1:1.
/// When the selection tool is active, dragging creates, moves or resizes the
/// document's selection; hit-testing and the resize/move maths live in
/// core/selection.h, and this widget only converts between widget and image
/// coordinates. The selection is drawn as a dashed outline with eight handles.
///
/// Thread-safety: main thread only (a Gtk::Widget).
class ImageCanvas : public Gtk::Widget {
public:
    ImageCanvas();
    ~ImageCanvas() override;

    /// Sets the document to draw and edit. The document is not owned and must
    /// outlive the canvas. Pass nullptr to draw nothing.
    void set_document(ImageDocument* document);

    /// Requests a redraw using the document's current state (e.g. after a
    /// rotation or a new image).
    void refresh();

    /// Enables or disables region selection. Disabling drops any in-progress
    /// drag and clears the pointer cursor; it does not clear the selection.
    void set_selection_tool_active(bool active);
    [[nodiscard]] bool selection_tool_active() const noexcept { return selection_tool_active_; }

    /// Emitted after the canvas changes the selection (during a drag), so that
    /// dependent UI such as an Export button can update.
    sigc::signal<void()>& signal_selection_changed();

protected:
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;

private:
    /// The rectangle in widget coordinates that the image is drawn into,
    /// preserving aspect ratio and centering it. Zero-sized when nothing can be
    /// drawn.
    [[nodiscard]] Gdk::Graphene::Rect image_destination_rect() const;

    /// Widget pixels per image pixel, or 1.0 when nothing is drawn.
    [[nodiscard]] double display_scale() const;

    /// Converts a point in widget coordinates to image coordinates.
    [[nodiscard]] PixelPoint widget_to_image(PixelPoint widget_point) const;

    /// The grab radius (in image pixels) used when hit-testing handles.
    [[nodiscard]] double handle_radius_image_pixels() const;

    void on_drag_begin(double start_x, double start_y);
    void on_drag_update(double offset_x, double offset_y);
    void on_drag_end();
    void on_motion(double x, double y);
    void on_leave();
    void update_cursor(SelectionHandle handle);

    /// Draws the dashed selection outline and its eight handles.
    void draw_selection_overlay(const Glib::RefPtr<Gtk::Snapshot>& snapshot) const;

    ImageDocument* document_ = nullptr;
    bool selection_tool_active_ = false;

    bool dragging_ = false;
    SelectionHandle active_handle_ = SelectionHandle::None;
    PixelPoint drag_start_widget_{};
    PixelPoint drag_anchor_image_{};
    ImageRectangle drag_start_region_{};

    sigc::signal<void()> selection_changed_signal_;
};

} // namespace imagemgr
