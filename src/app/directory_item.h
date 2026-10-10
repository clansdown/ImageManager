// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/object.h>
#include <glibmm/refptr.h>
#include <sigc++/sigc++.h>

namespace imagemgr {

/// Model item for one image file in the directory grid.
///
/// A C++-only Glib::Object so it can live in a Gio::ListStore and be bound by
/// the grid's list-item factory. It caches the thumbnail once generated and
/// emits signal_thumbnail_changed() when that changes, so a bound cell can
/// repaint itself (GTK does not re-bind a list item whose pointer is unchanged).
///
/// Thread-safety: main thread only (it is stored in a Gio::ListStore).
class DirectoryItem : public Glib::Object {
public:
    /// Creates an item for the given file path, display name, modification
    /// time (seconds since the Unix epoch, 0 when unknown) and number of
    /// recorded exports of that file.
    [[nodiscard]] static Glib::RefPtr<DirectoryItem> create(std::string path,
                                                            std::string display_name,
                                                            std::int64_t modified_unix_seconds,
                                                            std::size_t export_count);

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& display_name() const noexcept { return display_name_; }

    /// Last modification time in seconds since the Unix epoch (0 when unknown).
    [[nodiscard]] std::int64_t modified_unix_seconds() const noexcept {
        return modified_unix_seconds_;
    }

    /// Updates the last modification time, e.g. after the file was rewritten.
    void set_modified_unix_seconds(std::int64_t modified_unix_seconds) noexcept {
        modified_unix_seconds_ = modified_unix_seconds;
    }

    /// Case-insensitive, locale-aware collation key for display_name_, computed
    /// once so sorting does not re-fold the name on every comparison.
    [[nodiscard]] const std::string& name_sort_key() const noexcept { return name_sort_key_; }

    [[nodiscard]] bool has_thumbnail() const noexcept { return thumbnail_ != nullptr; }
    [[nodiscard]] const Glib::RefPtr<Gdk::Pixbuf>& thumbnail() const noexcept {
        return thumbnail_;
    }

    /// Sets the cached thumbnail and notifies observers. A null pixbuf records a
    /// failed decode.
    void set_thumbnail(Glib::RefPtr<Gdk::Pixbuf> thumbnail);

    /// How many exports have been recorded for this file.
    [[nodiscard]] std::size_t export_count() const noexcept { return export_count_; }

    /// Sets the recorded export count and notifies observers when it changed.
    void set_export_count(std::size_t count);

    /// Emitted after set_thumbnail(). A bound ThumbnailCell connects to this to
    /// refresh itself; the connection is dropped when the cell is unbound or
    /// destroyed.
    [[nodiscard]] sigc::signal<void()>& signal_thumbnail_changed();

    /// Emitted after set_export_count() when the count changed.
    [[nodiscard]] sigc::signal<void()>& signal_export_count_changed();

protected:
    DirectoryItem(std::string path, std::string display_name, std::int64_t modified_unix_seconds,
                  std::size_t export_count);

private:
    std::string path_;
    std::string display_name_;
    std::int64_t modified_unix_seconds_ = 0;
    std::string name_sort_key_;
    std::size_t export_count_ = 0;
    Glib::RefPtr<Gdk::Pixbuf> thumbnail_;
    sigc::signal<void()> thumbnail_changed_signal_;
    sigc::signal<void()> export_count_changed_signal_;
};

} // namespace imagemgr
