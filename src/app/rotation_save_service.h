// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <condition_variable>
#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <gdkmm/pixbuf.h>
#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>

#include "core/error.h"
#include "core/rotation.h"

namespace imagemgr {

/// Persists a rotation out to a file on a background thread and delivers the
/// result on the main thread.
///
/// Only the most recent request matters: save() supersedes any pending or
/// in-flight save, and superseded results are dropped. This suits a discrete
/// rotate action on a single-image view.
///
/// Construction/destruction and save() must run on the main thread (the thread
/// that created the service); the callback always runs there.
class RotationSaveService {
public:
    /// Called on the main thread with the requested path and rotation, and
    /// either success or an error.
    using SaveCallback = std::function<void(const std::string& path, QuarterTurns turns,
                                            std::expected<void, Error> result)>;

    /// @param callback Invoked on the main thread for every delivered result.
    ///        Must outlive the service.
    explicit RotationSaveService(SaveCallback callback);

    /// Stops and joins the worker thread. Must run on the main thread.
    ~RotationSaveService();

    RotationSaveService(const RotationSaveService&) = delete;
    RotationSaveService& operator=(const RotationSaveService&) = delete;

    /// Requests that path's rotation by turns be written, using rotated_pixbuf
    /// (the already-rotated image) for the lossless pixel formats. Supersedes
    /// any earlier request.
    void save(const std::string& path, QuarterTurns turns,
              Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf);

private:
    struct Request {
        std::string path;
        QuarterTurns turns = QuarterTurns::None;
        Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf;
        std::uint64_t generation = 0;
    };

    struct Completed {
        std::string path;
        QuarterTurns turns = QuarterTurns::None;
        std::uint64_t generation = 0;
        std::expected<void, Error> result;
    };

    void worker_loop();
    void deliver_completed();

    SaveCallback callback_;
    Glib::Dispatcher dispatcher_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<Request> pending_;
    std::optional<Completed> completed_;
    std::uint64_t generation_ = 0; // only written on the main thread
    bool stopping_ = false;
    std::thread worker_;
};

} // namespace imagemgr
