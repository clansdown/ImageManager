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

namespace imagemgr {

/// Decodes image files on a background thread and delivers the result on the
/// main thread.
///
/// Only the most recent request matters: load() supersedes any pending or
/// in-flight request, and results belonging to superseded requests are dropped.
/// This suits a single-image view where rapid selection changes should not
/// queue up work.
///
/// Construction/destruction and load()/cancel() must run on the main thread
/// (the thread that created the service). The callback always runs there.
class ImageLoadService {
public:
    /// Called on the main thread with the requested path and either the decoded
    /// full-resolution pixbuf or an error.
    using LoadCallback = std::function<void(
        const std::string& path,
        std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> result)>;

    /// @param callback Invoked on the main thread for every delivered result.
    ///        Must outlive the service.
    explicit ImageLoadService(LoadCallback callback);

    /// Stops and joins the worker thread. Must run on the main thread.
    ~ImageLoadService();

    ImageLoadService(const ImageLoadService&) = delete;
    ImageLoadService& operator=(const ImageLoadService&) = delete;

    /// Requests decoding of file_path, superseding any earlier request.
    void load(const std::string& file_path);

    /// Drops any pending or in-flight request; its result will be ignored.
    void cancel();

private:
    struct Request {
        std::string path;
        std::uint64_t generation;
    };

    struct Completed {
        std::string path;
        std::uint64_t generation;
        std::expected<Glib::RefPtr<Gdk::Pixbuf>, Error> result;
    };

    void worker_loop();
    void deliver_completed();

    LoadCallback callback_;
    Glib::Dispatcher dispatcher_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<Request> pending_;
    std::optional<Completed> completed_;
    std::uint64_t generation_ = 0;
    bool stopping_ = false;
    std::thread worker_;
};

} // namespace imagemgr
