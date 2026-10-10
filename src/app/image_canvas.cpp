// SPDX-License-Identifier: GPL-3.0-only

#include "app/image_canvas.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <cairomm/context.h>
#include <gdkmm/cursor.h>
#include <gdkmm/texture.h>
#include <glibmm/miscutils.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gesturedrag.h>
#include <gtkmm/snapshot.h>
#include <pangomm/font.h>
#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "io/image_document.h"

namespace imagemgr {

namespace {

// Grab radius around a handle, in widget pixels.
constexpr double kHandleRadiusWidgetPixels = 6.0;

// Drawn size of a selection handle, in widget pixels.
constexpr double kHandleSizeWidgetPixels = 8.0;

// Exported-region overlay, in widget pixels. The translucent fill marks the
// area, while a bright green border over a dark halo makes it read as an
// outlined box (rather than a merely lightened patch) on both light and dark
// images. The label is inset so its backing clears the border.
constexpr double kExportBorderWidth = 2.0;
constexpr double kExportBorderHaloWidth = 4.0;
constexpr double kExportLabelInset = 6.0;
constexpr double kExportLabelPadding = 3.0;

} // namespace

ImageCanvas::ImageCanvas() {
    // Fill whatever space the parent gives us so the image can be centered.
    set_hexpand(true);
    set_vexpand(true);

    auto drag = Gtk::GestureDrag::create();
    drag->signal_drag_begin().connect(
        [this](double start_x, double start_y) { on_drag_begin(start_x, start_y); });
    drag->signal_drag_update().connect(
        [this](double offset_x, double offset_y) { on_drag_update(offset_x, offset_y); });
    drag->signal_drag_end().connect(
        [this](double /*offset_x*/, double /*offset_y*/) { on_drag_end(); });
    add_controller(drag);

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this](double x, double y) { on_motion(x, y); });
    motion->signal_leave().connect([this] { on_leave(); });
    add_controller(motion);
}

ImageCanvas::~ImageCanvas() = default;

void ImageCanvas::set_document(ImageDocument* document) {
    document_ = document;
    queue_draw();
}

void ImageCanvas::refresh() {
    queue_draw();
}

sigc::signal<void()>& ImageCanvas::signal_selection_changed() {
    return selection_changed_signal_;
}

void ImageCanvas::set_selection_tool_active(bool active) {
    selection_tool_active_ = active;
    if (!active) {
        dragging_ = false;
        active_handle_ = SelectionHandle::None;
        set_cursor(Glib::RefPtr<Gdk::Cursor>{});
    } else {
        set_cursor("crosshair");
    }
}

Gdk::Graphene::Rect ImageCanvas::image_destination_rect() const {
    const int widget_width = get_width();
    const int widget_height = get_height();
    if (document_ == nullptr || !document_->has_image() || widget_width <= 0 ||
        widget_height <= 0) {
        return Gdk::Graphene::Rect(0.0F, 0.0F, 0.0F, 0.0F);
    }

    const auto& pixbuf = document_->display_pixbuf();
    if (pixbuf == nullptr) {
        return Gdk::Graphene::Rect(0.0F, 0.0F, 0.0F, 0.0F);
    }
    const int image_width = pixbuf->get_width();
    const int image_height = pixbuf->get_height();
    if (image_width <= 0 || image_height <= 0) {
        return Gdk::Graphene::Rect(0.0F, 0.0F, 0.0F, 0.0F);
    }

    // Fit to the widget, but never enlarge a small image past 1:1.
    const double width_scale = static_cast<double>(widget_width) / image_width;
    const double height_scale = static_cast<double>(widget_height) / image_height;
    const double scale = std::min({width_scale, height_scale, 1.0});

    const double draw_width = image_width * scale;
    const double draw_height = image_height * scale;
    const double x = (widget_width - draw_width) / 2.0;
    const double y = (widget_height - draw_height) / 2.0;

    return Gdk::Graphene::Rect(static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(draw_width),
                               static_cast<float>(draw_height));
}

double ImageCanvas::display_scale() const {
    const Gdk::Graphene::Rect destination = image_destination_rect();
    if (document_ == nullptr || !document_->has_image() || destination.get_width() <= 0.0F) {
        return 1.0;
    }
    const int image_width = document_->display_pixbuf()->get_width();
    if (image_width <= 0) {
        return 1.0;
    }
    return static_cast<double>(destination.get_width()) / image_width;
}

PixelPoint ImageCanvas::widget_to_image(PixelPoint widget_point) const {
    const Gdk::Graphene::Rect destination = image_destination_rect();
    const double scale = display_scale();
    return PixelPoint{.x = (widget_point.x - destination.get_x()) / scale,
                      .y = (widget_point.y - destination.get_y()) / scale};
}

double ImageCanvas::handle_radius_image_pixels() const {
    const double scale = display_scale();
    return scale > 0.0 ? kHandleRadiusWidgetPixels / scale : kHandleRadiusWidgetPixels;
}

void ImageCanvas::on_drag_begin(double start_x, double start_y) {
    if (!selection_tool_active_ || document_ == nullptr || !document_->has_image()) {
        return;
    }

    drag_start_widget_ = PixelPoint{.x = start_x, .y = start_y};
    drag_anchor_image_ = widget_to_image(drag_start_widget_);

    SelectionHandle handle = SelectionHandle::None;
    if (document_->has_selection()) {
        handle = hit_test_selection(document_->selection(), drag_anchor_image_,
                                    handle_radius_image_pixels());
    }

    if (handle == SelectionHandle::None) {
        // Start a new selection anchored at the press point; dragging out from
        // it resizes the bottom-right corner.
        drag_start_region_ = ImageRectangle{.x = static_cast<int>(std::lround(drag_anchor_image_.x)),
                                            .y = static_cast<int>(std::lround(drag_anchor_image_.y)),
                                            .width = 0,
                                            .height = 0};
        active_handle_ = SelectionHandle::BottomRight;
        document_->set_selection(drag_start_region_);
    } else {
        drag_start_region_ = document_->selection();
        active_handle_ = handle;
    }

    dragging_ = true;
}

void ImageCanvas::on_drag_update(double offset_x, double offset_y) {
    if (!dragging_ || document_ == nullptr || !document_->has_image()) {
        return;
    }

    const PixelPoint current_widget{.x = drag_start_widget_.x + offset_x,
                                    .y = drag_start_widget_.y + offset_y};
    const PixelPoint current_image = widget_to_image(current_widget);
    const ImageSize bounds = document_->display_size();

    ImageRectangle region;
    if (active_handle_ == SelectionHandle::Inside) {
        const PixelPoint delta{.x = current_image.x - drag_anchor_image_.x,
                               .y = current_image.y - drag_anchor_image_.y};
        region = move_selection(drag_start_region_, delta, bounds);
    } else {
        region = resize_selection(drag_start_region_, active_handle_, current_image, bounds);
    }

    document_->set_selection(region);
    queue_draw();
    selection_changed_signal_.emit();
}

void ImageCanvas::on_drag_end() {
    dragging_ = false;
    active_handle_ = SelectionHandle::None;
}

void ImageCanvas::on_motion(double x, double y) {
    if (document_ == nullptr || !document_->has_image()) {
        update_cursor(SelectionHandle::None);
        return;
    }

    SelectionHandle handle = SelectionHandle::None;
    if (document_->has_selection()) {
        handle = hit_test_selection(document_->selection(), widget_to_image(PixelPoint{x, y}),
                                    handle_radius_image_pixels());
    }
    update_cursor(handle);
}

void ImageCanvas::on_leave() {
    update_cursor(SelectionHandle::None);
}

void ImageCanvas::update_cursor(SelectionHandle handle) {
    if (!selection_tool_active_) {
        set_cursor(Glib::RefPtr<Gdk::Cursor>{});
        return;
    }

    switch (handle) {
    case SelectionHandle::TopLeft:
    case SelectionHandle::BottomRight:
        set_cursor("nwse-resize");
        break;
    case SelectionHandle::TopRight:
    case SelectionHandle::BottomLeft:
        set_cursor("nesw-resize");
        break;
    case SelectionHandle::Top:
    case SelectionHandle::Bottom:
        set_cursor("ns-resize");
        break;
    case SelectionHandle::Left:
    case SelectionHandle::Right:
        set_cursor("ew-resize");
        break;
    case SelectionHandle::Inside:
        set_cursor("move");
        break;
    case SelectionHandle::None:
        set_cursor("crosshair");
        break;
    }
}

void ImageCanvas::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) {
    const Gdk::Graphene::Rect destination = image_destination_rect();
    if (destination.get_width() > 0.0F && destination.get_height() > 0.0F) {
        const auto& pixbuf = document_->display_pixbuf();
        const auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);
        snapshot->append_texture(texture, destination);
    }
    draw_export_overlay(snapshot);
    draw_selection_overlay(snapshot);
}

Gdk::Graphene::Rect ImageCanvas::image_rect_to_widget(ImageRectangle rectangle) const {
    const Gdk::Graphene::Rect destination = image_destination_rect();
    const double scale = display_scale();
    return Gdk::Graphene::Rect(static_cast<float>(destination.get_x() + rectangle.x * scale),
                               static_cast<float>(destination.get_y() + rectangle.y * scale),
                               static_cast<float>(rectangle.width * scale),
                               static_cast<float>(rectangle.height * scale));
}

void ImageCanvas::draw_selection_overlay(const Glib::RefPtr<Gtk::Snapshot>& snapshot) const {
    if (document_ == nullptr || !document_->has_selection()) {
        return;
    }

    const Gdk::Graphene::Rect destination = image_destination_rect();
    const double scale = display_scale();
    if (destination.get_width() <= 0.0F || scale <= 0.0) {
        return;
    }

    const ImageRectangle selection = document_->selection();
    const Gdk::Graphene::Rect selection_box = image_rect_to_widget(selection);
    const double left = selection_box.get_x();
    const double top = selection_box.get_y();
    const double width = selection_box.get_width();
    const double height = selection_box.get_height();

    const auto context = snapshot->append_cairo(Gdk::Graphene::Rect(
        0.0F, 0.0F, static_cast<float>(get_width()), static_cast<float>(get_height())));
    if (!context) {
        return;
    }

    context->save();

    // Dark solid underlay so the dashed outline reads on any image.
    context->set_line_width(1.0);
    context->set_source_rgba(0.0, 0.0, 0.0, 0.6);
    context->rectangle(left, top, width, height);
    context->stroke();

    // White dashed outline.
    context->set_dash(std::vector<double>{4.0, 4.0}, 0.0);
    context->set_source_rgba(1.0, 1.0, 1.0, 0.95);
    context->rectangle(left, top, width, height);
    context->stroke();
    context->set_dash(std::vector<double>{}, 0.0);

    // Eight handles.
    const double half = kHandleSizeWidgetPixels / 2.0;
    const std::array<PixelPoint, 8> handles{{
        {left, top},
        {left + width / 2.0, top},
        {left + width, top},
        {left + width, top + height / 2.0},
        {left + width, top + height},
        {left + width / 2.0, top + height},
        {left, top + height},
        {left, top + height / 2.0},
    }};
    for (const PixelPoint& handle : handles) {
        context->rectangle(handle.x - half, handle.y - half, kHandleSizeWidgetPixels,
                           kHandleSizeWidgetPixels);
        context->set_source_rgba(1.0, 1.0, 1.0, 1.0);
        context->fill_preserve();
        context->set_line_width(1.0);
        context->set_source_rgba(0.0, 0.0, 0.0, 0.8);
        context->stroke();
    }

    context->restore();
}

void ImageCanvas::draw_export_overlay(const Glib::RefPtr<Gtk::Snapshot>& snapshot) const {
    if (document_ == nullptr || document_->exported_regions().empty()) {
        return;
    }
    const Gdk::Graphene::Rect destination = image_destination_rect();
    if (destination.get_width() <= 0.0F || destination.get_height() <= 0.0F) {
        return;
    }

    // One box per distinct region, keeping the most recent export's file name.
    std::vector<const ExportRecord*> distinct;
    for (const ExportRecord& record : document_->exported_regions()) {
        const auto existing =
            std::find_if(distinct.begin(), distinct.end(), [&record](const ExportRecord* other) {
                return other->region == record.region;
            });
        if (existing == distinct.end()) {
            distinct.push_back(&record);
        } else if (record.exported_unix_seconds >= (*existing)->exported_unix_seconds) {
            *existing = &record;
        }
    }

    const auto context = snapshot->append_cairo(Gdk::Graphene::Rect(
        0.0F, 0.0F, static_cast<float>(get_width()), static_cast<float>(get_height())));
    if (!context) {
        return;
    }

    context->save();
    for (const ExportRecord* record : distinct) {
        // Stored regions are in the file's pixel space; a view-only rotation
        // (for formats that cannot be written) is applied here for display.
        const Gdk::Graphene::Rect box =
            image_rect_to_widget(document_->source_region_to_display(record->region));
        if (box.get_width() <= 0.0F || box.get_height() <= 0.0F) {
            continue;
        }

        // Mostly transparent light-grey fill marking the exported area.
        context->set_source_rgba(0.85, 0.85, 0.85, 0.25);
        context->rectangle(box.get_x(), box.get_y(), box.get_width(), box.get_height());
        context->fill();

        // Distinctive border: a wide dark halo underneath a bright green line,
        // leaving a thin dark outline around the green so the box reads as an
        // outline on any image, not just a lightened patch.
        context->set_source_rgba(0.0, 0.0, 0.0, 0.55);
        context->set_line_width(kExportBorderHaloWidth);
        context->rectangle(box.get_x(), box.get_y(), box.get_width(), box.get_height());
        context->stroke();
        context->set_source_rgb(0.2, 0.9, 0.2);
        context->set_line_width(kExportBorderWidth);
        context->rectangle(box.get_x(), box.get_y(), box.get_width(), box.get_height());
        context->stroke();

        // The output file name, inset from the border on a dark backing so it
        // stays readable over a dark image.
        auto layout = Pango::Layout::create(context);
        layout->set_text(Glib::path_get_basename(record->output_path));
        Pango::FontDescription font;
        font.set_size(9 * Pango::SCALE);
        layout->set_font_description(font);

        int label_width = 0;
        int label_height = 0;
        layout->get_pixel_size(label_width, label_height);
        const double label_x = box.get_x() + kExportLabelInset;
        const double label_y = box.get_y() + kExportLabelInset;
        context->set_source_rgba(0.0, 0.0, 0.0, 0.55);
        context->rectangle(label_x - kExportLabelPadding, label_y - kExportLabelPadding,
                           static_cast<double>(label_width) + 2.0 * kExportLabelPadding,
                           static_cast<double>(label_height) + 2.0 * kExportLabelPadding);
        context->fill();

        context->set_source_rgba(1.0, 1.0, 1.0, 0.95);
        context->move_to(label_x, label_y);
        layout->show_in_cairo_context(context);
    }
    context->restore();
}

} // namespace imagemgr
