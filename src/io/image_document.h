// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <vector>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "core/export_history.h"
#include "core/geometry.h"
#include "core/rotation.h"

namespace imagemgr {

/// Model for the image shown in the right pane.
///
/// Holds the decoded source pixbuf together with the non-destructive rotation
/// and selection state. The source is never modified: display_pixbuf() returns
/// the source rotated by rotation(), cached and recomputed only when the
/// rotation changes, so repeated draws are cheap.
///
/// The selection is expressed in display (rotated) pixel coordinates and is
/// rotated in step with the image, so it always refers to the same visual
/// region.
///
/// Thread-safety: main thread only (it holds a Gdk::Pixbuf and is drawn by
/// widgets).
class ImageDocument {
public:
    /// Installs a newly decoded image and resets rotation and selection.
    /// @param source_path Path the pixbuf was decoded from, kept for display
    ///        and for choosing the default export format.
    /// @param source_pixbuf The unrotated, full-resolution image.
    void set_source(std::string source_path, Glib::RefPtr<Gdk::Pixbuf> source_pixbuf);

    /// Drops the image and all derived state.
    void clear();

    [[nodiscard]] bool has_image() const noexcept { return source_pixbuf_ != nullptr; }
    [[nodiscard]] const std::string& source_path() const noexcept { return source_path_; }

    /// The source rotated by rotation(); null when there is no image.
    [[nodiscard]] const Glib::RefPtr<Gdk::Pixbuf>& display_pixbuf() const noexcept {
        return display_pixbuf_;
    }

    /// Size in pixels of the display (rotated) image; zero when there is none.
    [[nodiscard]] ImageSize display_size() const noexcept;

    /// Size in pixels of the unrotated source image; zero when there is none.
    [[nodiscard]] ImageSize source_size() const noexcept;

    /// Maps a rectangle in the source (unrotated) pixel space to the current
    /// display (rotated) space.
    [[nodiscard]] ImageRectangle source_region_to_display(ImageRectangle source_region) const;

    /// Maps a rectangle in the current display space back to the source pixel
    /// space.
    [[nodiscard]] ImageRectangle display_region_to_source(ImageRectangle display_region) const;

    [[nodiscard]] QuarterTurns rotation() const noexcept { return rotation_; }

    /// Rotates the display 90 degrees; the selection turns with it.
    void rotate_clockwise();

    /// Rotates the display 90 degrees the other way; the selection turns too.
    void rotate_counter_clockwise();

    /// Returns to the unrotated source, rotating the selection back with it.
    void reset_rotation();

    /// Bakes the current display rotation into the source: the display becomes
    /// the source, the rotation resets to none, and the recorded export regions
    /// are rotated into the new frame. Call after a rotation has been written
    /// out to the file.
    void commit_rotation();

    [[nodiscard]] bool has_selection() const noexcept { return has_selection_; }

    /// The selection in display pixel coordinates; meaningless unless
    /// has_selection() is true.
    [[nodiscard]] ImageRectangle selection() const noexcept { return selection_; }

    /// Sets the selection, clamped to the display bounds.
    void set_selection(ImageRectangle selection);

    void clear_selection();

    /// Replaces the recorded exports for this image, e.g. after loading the
    /// persistent history when the image is opened.
    void set_exported_regions(std::vector<ExportRecord> regions);

    /// Records one more export of this image (a live export) for the overlay.
    void add_exported_region(ExportRecord region);

    /// The recorded exports, in the order they were recorded.
    [[nodiscard]] const std::vector<ExportRecord>& exported_regions() const noexcept {
        return exported_regions_;
    }

private:
    /// Recomputes display_pixbuf_ from source_pixbuf_ and rotation_.
    void rebuild_display_pixbuf();

    std::string source_path_;
    Glib::RefPtr<Gdk::Pixbuf> source_pixbuf_;
    Glib::RefPtr<Gdk::Pixbuf> display_pixbuf_;
    QuarterTurns rotation_ = QuarterTurns::None;
    ImageRectangle selection_{};
    bool has_selection_ = false;
    std::vector<ExportRecord> exported_regions_;
};

} // namespace imagemgr
