#pragma once

#include "ezmk/util.hpp"  // for EZMK_WIN / EZMK_MACOS / EZMK_LINUX platform macros

// Platform aliases for internal use (reuse util.hpp definitions).
#if defined(EZMK_WIN)
  #define EZMK_FILEWATCHER_WIN 1
#elif defined(EZMK_MACOS)
  #define EZMK_FILEWATCHER_MACOS 1
#elif defined(EZMK_LINUX)
  #define EZMK_FILEWATCHER_LINUX 1
#endif

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

namespace ezmk::util {
namespace fs = std::filesystem;

// Cross-platform file system watcher.
// Monitors directories for file changes and invokes a callback after
// a debounce window (300ms by default) to coalesce rapid edits.
//
// Recursion (1.1.3 C4): platform-dependent and NOT configurable.
//   - Windows: watches the directory subtree (ReadDirectoryChangesW bWatchSubtree=TRUE).
//   - Linux/macOS: watches the directory itself only (non-recursive).
// The old recursive_ field was dead code (never read) and has been removed;
// the add_directory(recursive) parameter is kept for API compatibility but
// is currently not honored.
//
// Event granularity (1.2.0-dev.11):
//   - Windows/Linux: the callback receives the specific changed file path.
//   - macOS: the callback receives the watched DIRECTORY path — kqueue
//     EVFILT_VNODE reports the vnode, not the changed entry, and per-file
//     events are not delivered. Do not rely on per-file paths on macOS;
//     FSEvents-based per-file reporting is a separate deferred work item.
//
// 1.4.2 additions:
//   - F-32: ignore prefixes/suffixes (build artifacts) and an existence filter
//     at flush time, so rebuilds provoked by the watcher's own outputs do not
//     self-trigger. Ignore rules are read by both the platform event pumps and
//     flush_pending(); set them before run().
//   - F-31: when a watched directory disappears at runtime, its platform watch
//     is dropped and re-established by a low-frequency repair pass in run()
//     once the directory is back.
//   - F-05: a platform worker that dies on a hard error records it; run() then
//     warns and returns instead of blocking forever (watch 假死).
//
// Usage:
//   FileWatcher watcher([](const fs::path& p) { ... });
//   watcher.add_directory("/path/to/src");
//   watcher.add_ignore_prefix("/path/to/build/");
//   watcher.add_ignore_suffix(".o");
//   watcher.run();  // blocks until stop() is called from another thread
//
// Thread safety: add_directory() / add_ignore_*() must be called before run().
// stop() is safe to call from any thread (e.g. SIGINT handler).
// had_worker_error() / worker_error_message() are safe to read from any thread
// while run() is executing.
class FileWatcher {
public:
    using Callback = std::function<void(const fs::path&)>;

    FileWatcher(Callback cb, int debounce_ms = 300);
    ~FileWatcher();

    // Add a directory to watch. Must be called before run().
    void add_directory(const fs::path& dir, bool recursive = true);

    // 1.4.2 F-32: ignore any event whose absolute path starts with `prefix`
    // (directory) or ends with `suffix` (file extension). Relative prefixes are
    // resolved against the current directory; matching is case-insensitive on
    // Windows. Must be called before run().
    void add_ignore_prefix(const std::string& prefix);
    void add_ignore_suffix(const std::string& suffix);

    // Blocking event loop. Returns when stop() is called, or when a platform
    // worker died on a hard error (F-05 — see had_worker_error()).
    void run();

    // Signal the event loop to stop. Thread-safe.
    void stop();

    // 1.4.2 F-05: true once a platform worker exited on a hard error. run()
    // returns shortly after; callers should warn and stop waiting.
    bool had_worker_error() const { return worker_error_.load(); }
    std::string worker_error_message() const;

    // Non-copyable, non-movable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

private:
    void process_events();
    void flush_pending();
    void check_and_flush();

    // 1.4.2: shared run() main loop (debounce + worker-death check + repair).
    void main_loop();
    // Record a platform event (ignore-filtered) into the debounce set.
    void note_event(const fs::path& p);
    // Does the absolute generic path `abs_generic` hit an ignore rule?
    bool is_ignored(const std::string& abs_generic) const;
    // Platform worker hard-error channel (F-05).
    void report_worker_error(const std::string& msg);

    Callback callback_;
    int debounce_ms_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Directories to watch
    std::vector<fs::path> dirs_;

    // 1.4.2 F-32: ignore rules (see add_ignore_prefix/add_ignore_suffix).
    std::vector<std::string> ignore_prefixes_;
    std::vector<std::string> ignore_suffixes_;

    // 1.4.2 F-05: platform worker death is recorded here.
    std::atomic<bool> worker_error_{false};
    mutable std::mutex worker_error_mutex_;
    std::string worker_error_msg_;

    // Debounce state
    std::mutex pending_mutex_;
    std::unordered_set<std::string> pending_paths_;
    std::chrono::steady_clock::time_point last_event_;

    // Platform-specific state
#ifdef EZMK_FILEWATCHER_WIN
    // Windows: I/O Completion Port + ReadDirectoryChangesW
    void* iocp_;           // HANDLE
    std::thread worker_;
    struct WatchEntry {
        std::string dir_path;
        void* dir_handle;  // HANDLE
        std::vector<uint8_t> buffer;
        void* overlapped;  // OVERLAPPED* — lifetime owned by overlapped_pool_
        bool armed = false;      // 1.4.2 F-31: a posted read is pending for it
        bool lost_warned = false; // 1.4.2 F-31: one warning per loss
        bool repaired = false;    // 1.4.2 F-31: directory came back (info log)
    };
    std::vector<WatchEntry> watches_;
    // 1.1.3 C3: OVERLAPPED 池实例化——每个 FileWatcher 自持生命周期。旧实现是文件级
    // 全局，多实例时第二个的 win32_cleanup() 会令第一个的 OVERLAPPED 悬垂。
    std::vector<void*> overlapped_pool_;  // OVERLAPPED* (raw; deleted in win32_cleanup)
    std::mutex platform_mutex_;  // guards watches_ (worker vs run()'s repair)

    void win32_worker();
    bool win32_add_watch(const fs::path& dir);
    bool win32_arm(WatchEntry& w);
    size_t win32_index_of_overlapped(void* ov) const;
    void win32_repair_watches();
    void win32_cleanup();
#elif defined(EZMK_FILEWATCHER_LINUX)
    // Linux: inotify
    int inotify_fd_ = -1;
    std::unordered_map<int, std::string> wd_to_path_;  // watch descriptor → path
    std::thread worker_;
    std::mutex platform_mutex_;            // guards wd_to_path_ (worker vs repair)
    std::set<std::string> repair_warned_;  // 1.4.2 F-31: one warning per loss

    void linux_worker();
    void linux_repair_watches();
    void linux_cleanup();
#elif defined(EZMK_FILEWATCHER_MACOS)
    // macOS: kqueue
    int kq_ = -1;
    std::thread worker_;
    struct KqWatch {
        std::string path;
        int fd = -1;
        bool lost = false;  // 1.4.2 F-31: vnode deleted/renamed away
    };
    std::vector<KqWatch> watches_;
    std::mutex platform_mutex_;            // guards watches_ (worker vs repair)
    std::set<std::string> repair_warned_;  // 1.4.2 F-31: one warning per loss

    void macos_worker();
    bool macos_register(KqWatch& w);
    void macos_repair_watches();
    void macos_cleanup();
#endif
};

} // namespace ezmk::util
