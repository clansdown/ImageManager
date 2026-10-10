// SPDX-License-Identifier: GPL-3.0-only

#pragma once

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
    /// Creates an item for the given file path, display name and modification
    /// time (seconds since the Unix epoch, 0 when unknown).
    [[nodiscard]] static Glib::RefPtr<DirectoryItem> create(std::string path,
                                                            std::string display_name,
                                                            std::int64_t modified_unix_seconds);

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& display_name() const noexcept { return display_name_; }

    /// Last modification time in seconds since the Unix epoch (0 when unknown).
    [[nodiscard]] std::int64_t modified_unix_seconds() const noexcept {
        return modified_unix_seconds_;
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

    /// Emitted after set_thumbnail(). A bound ThumbnailCell connects to this to
    /// refresh itself; the connection is dropped when the cell is unbound or
    /// destroyed.
    [[nodiscard]] sigc::signal<void()>& signal_thumbnail_changed();

protected:
    DirectoryItem(std::string path, std::string display_name, std::int64_t modified_unix_seconds);

private:
    std::string path_;
    std::string display_name_;
    std::int64_t modified_unix_seconds_ = 0;
    std::string name_sort_key_;
    Glib::RefPtr<Gdk::Pixbuf> thumbnail_;
    sigc::signal<void()> thumbnail_changed_signal_;
};

} // namespace imagemgr
