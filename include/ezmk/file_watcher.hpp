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
// Usage:
//   FileWatcher watcher([](const fs::path& p) { ... });
//   watcher.add_directory("/path/to/src");
//   watcher.add_directory("/path/to/include");
//   watcher.run();  // blocks until stop() is called from another thread
//
// Thread safety: add_directory() must be called before run().
// stop() is safe to call from any thread (e.g. SIGINT handler).
class FileWatcher {
public:
    using Callback = std::function<void(const fs::path&)>;

    FileWatcher(Callback cb, int debounce_ms = 300);
    ~FileWatcher();

    // Add a directory to watch. Must be called before run().
    void add_directory(const fs::path& dir, bool recursive = true);

    // Blocking event loop. Returns when stop() is called.
    void run();

    // Signal the event loop to stop. Thread-safe.
    void stop();

    // Non-copyable, non-movable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

private:
    void process_events();
    void flush_pending();
    void check_and_flush();

    Callback callback_;
    int debounce_ms_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Directories to watch
    std::vector<fs::path> dirs_;

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
    };
    std::vector<WatchEntry> watches_;
    // 1.1.3 C3: OVERLAPPED 池实例化——每个 FileWatcher 自持生命周期。旧实现是文件级
    // 全局，多实例时第二个的 win32_cleanup() 会令第一个的 OVERLAPPED 悬垂。
    std::vector<void*> overlapped_pool_;  // OVERLAPPED* (raw; deleted in win32_cleanup)

    void win32_worker();
    void win32_add_watch(const fs::path& dir);
    void win32_cleanup();
#elif defined(EZMK_FILEWATCHER_LINUX)
    // Linux: inotify
    int inotify_fd_ = -1;
    std::unordered_map<int, std::string> wd_to_path_;  // watch descriptor → path
    std::thread worker_;

    void linux_worker();
    void linux_cleanup();
#elif defined(EZMK_FILEWATCHER_MACOS)
    // macOS: kqueue
    int kq_ = -1;
    std::thread worker_;
    struct KqWatch {
        std::string path;
        int fd = -1;
    };
    std::vector<KqWatch> watches_;

    void macos_worker();
    void macos_cleanup();
#endif
};

} // namespace ezmk::util
