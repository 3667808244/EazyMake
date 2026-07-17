#include "ezmk/util.hpp"
#include "ezmk/file_watcher.hpp"

#include <algorithm>
#include <set>

// Platform-specific includes (must match EZMK_FILEWATCHER_* macros defined in file_watcher.hpp)
#ifdef EZMK_FILEWATCHER_WIN
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#elif defined(EZMK_FILEWATCHER_LINUX)
  #include <sys/inotify.h>
  #include <sys/poll.h>
  #include <unistd.h>
  #include <limits.h>
#elif defined(EZMK_FILEWATCHER_MACOS)
  #include <sys/event.h>
  #include <sys/time.h>
  #include <fcntl.h>
  #include <unistd.h>
#endif

namespace ezmk::util {

// ===================================================================
// Common helpers
// ===================================================================

// Normalize a path to a canonical string key for dedup
static std::string path_key(const fs::path& p) {
    return fs::absolute(p).generic_string();
}

// ===================================================================
// Constructor / Destructor
// ===================================================================

FileWatcher::FileWatcher(Callback cb, int debounce_ms)
    : callback_(std::move(cb))
    , debounce_ms_(debounce_ms)
    , recursive_(true) {
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::add_directory(const fs::path& dir, bool recursive) {
    recursive_ = recursive;
    dirs_.push_back(fs::absolute(dir));
}

void FileWatcher::stop() {
    stop_requested_ = true;
}

// Debounce: accumulate changed paths, fire callback after debounce_ms of silence
void FileWatcher::flush_pending() {
    std::unordered_set<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (pending_paths_.empty()) return;
        paths.swap(pending_paths_);
        last_event_ = std::chrono::steady_clock::time_point{};
    }

    // Filter: only keep files that actually exist (ignore transient editor temp files)
    // and fire callback for each unique path
    for (auto& p : paths) {
        fs::path fp(p);
        // Skip editor temp/swap files
        auto ext = fp.extension().string();
        auto name = fp.filename().string();
        if (!name.empty() && name[0] == '.') continue;  // hidden files
        if (name.back() == '~') continue;                 // vim backup files
        if (ext == ".swp" || ext == ".swx" || ext == ".tmp") continue;

        callback_(fp);
    }
}

// Check if debounce window has elapsed and flush pending paths if so.
void FileWatcher::check_and_flush() {
    bool should_flush = false;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (!pending_paths_.empty() &&
            last_event_ != std::chrono::steady_clock::time_point{}) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_event_).count();
            if (elapsed >= debounce_ms_) {
                should_flush = true;
            }
        }
    }
    if (should_flush) {
        flush_pending();
    }
}

// ===================================================================
// Windows implementation (ReadDirectoryChangesW + IOCP)
// ===================================================================
#ifdef EZMK_FILEWATCHER_WIN

// RAII ownership for OVERLAPPED structs (raw pointers stored in WatchEntry).
static std::vector<std::unique_ptr<OVERLAPPED>> g_overlapped_pool;

void FileWatcher::win32_add_watch(const fs::path& dir) {
    if (!util::file_exists(dir)) {
        util::warn(std::string("watch directory not found, skipping: ") + dir.string());
        return;
    }

    // Open directory handle for overlapped I/O
    std::string dir_str = dir.string();
    std::wstring wdir(dir_str.begin(), dir_str.end());

    HANDLE hDir = CreateFileW(
        wdir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        util::warn(std::string("failed to open watch directory: ") + dir_str);
        return;
    }

    // Associate with IOCP
    if (CreateIoCompletionPort(hDir, iocp_, (ULONG_PTR)watches_.size(), 0) == nullptr) {
        CloseHandle(hDir);
        util::warn(std::string("failed to associate directory with IOCP: ") + dir_str);
        return;
    }

    WatchEntry entry;
    entry.dir_path = dir_str;
    entry.dir_handle = hDir;
    entry.buffer.resize(64 * 1024); // 64KB buffer

    // Allocate OVERLAPPED with RAII ownership
    auto ov = std::make_unique<OVERLAPPED>();
    entry.overlapped = ov.get();
    g_overlapped_pool.push_back(std::move(ov));

    watches_.push_back(std::move(entry));
}

void FileWatcher::win32_worker() {
    // Post initial ReadDirectoryChangesW for each watch
    for (size_t i = 0; i < watches_.size(); ++i) {
        auto& w = watches_[i];
        OVERLAPPED* ov = static_cast<OVERLAPPED*>(w.overlapped);
        DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                       FILE_NOTIFY_CHANGE_DIR_NAME |
                       FILE_NOTIFY_CHANGE_LAST_WRITE |
                       FILE_NOTIFY_CHANGE_SIZE;

        ReadDirectoryChangesW(
            w.dir_handle,
            w.buffer.data(),
            static_cast<DWORD>(w.buffer.size()),
            TRUE,  // watch subtree
            filter,
            nullptr,
            ov,
            nullptr
        );
    }

    // Event loop
    while (!stop_requested_) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* ov = nullptr;

        // Wait up to 500ms for IOCP events
        BOOL ok = GetQueuedCompletionStatus(
            iocp_,
            &bytesTransferred,
            &completionKey,
            &ov,
            500  // timeout ms
        );

        if (!ok && ov == nullptr) {
            // Timeout — check debounce
            if (stop_requested_) break;

            std::lock_guard<std::mutex> lock(pending_mutex_);
            if (!pending_paths_.empty()) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_event_).count();
                if (elapsed >= debounce_ms_) {
                    // Need to flush outside the lock; we use a flag
                    // Actually let's just flush from run()'s main loop
                }
            }
            continue;
        }

        if (completionKey < watches_.size() && bytesTransferred > 0) {
            auto& w = watches_[completionKey];
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(w.buffer.data());

            while (true) {
                // Convert wide-char filename to UTF-8 path
                std::wstring wfname(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::string fname(wfname.begin(), wfname.end());

                fs::path full_path = fs::path(w.dir_path) / fname;

                {
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    pending_paths_.insert(path_key(full_path));
                    last_event_ = std::chrono::steady_clock::now();
                }

                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
            }

            // Re-post the read
            OVERLAPPED* ov2 = static_cast<OVERLAPPED*>(w.overlapped);
            memset(ov2, 0, sizeof(OVERLAPPED));
            DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                           FILE_NOTIFY_CHANGE_DIR_NAME |
                           FILE_NOTIFY_CHANGE_LAST_WRITE |
                           FILE_NOTIFY_CHANGE_SIZE;

            ReadDirectoryChangesW(
                w.dir_handle,
                w.buffer.data(),
                static_cast<DWORD>(w.buffer.size()),
                TRUE, filter, nullptr, ov2, nullptr
            );
        }
    }
}

void FileWatcher::win32_cleanup() {
    for (auto& w : watches_) {
        if (w.dir_handle) {
            CancelIo(w.dir_handle);
            CloseHandle(w.dir_handle);
        }
    }
    watches_.clear();
    g_overlapped_pool.clear();  // RAII cleanup of OVERLAPPED allocations
    if (iocp_) {
        CloseHandle(iocp_);
        iocp_ = nullptr;
    }
}

#endif // EZMK_FILEWATCHER_WIN

// ===================================================================
// Linux implementation (inotify)
// ===================================================================
#ifdef EZMK_FILEWATCHER_LINUX

void FileWatcher::linux_worker() {
    char buf[4096 * 4] __attribute__((aligned(__alignof__(struct inotify_event))));

    while (!stop_requested_) {
        struct pollfd pfd;
        pfd.fd = inotify_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 500); // 500ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) {
            // Timeout — check debounce
            continue;
        }

        ssize_t len = read(inotify_fd_, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR) continue;
            break;
        }

        bool got_event = false;
        for (char* ptr = buf; ptr < buf + len; ) {
            auto* event = reinterpret_cast<struct inotify_event*>(ptr);
            if (event->len > 0) {
                auto it = wd_to_path_.find(event->wd);
                if (it != wd_to_path_.end()) {
                    fs::path full_path = fs::path(it->second) / event->name;

                    // Skip editor temp files
                    std::string fname = event->name;
                    if (!fname.empty() && (fname[0] == '.' || fname.back() == '~')) {
                        ptr += sizeof(struct inotify_event) + event->len;
                        continue;
                    }

                    {
                        std::lock_guard<std::mutex> lock(pending_mutex_);
                        pending_paths_.insert(path_key(full_path));
                        last_event_ = std::chrono::steady_clock::now();
                    }
                    got_event = true;
                }
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
}

void FileWatcher::linux_cleanup() {
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
}

#endif // EZMK_FILEWATCHER_LINUX

// ===================================================================
// macOS implementation (kqueue)
// ===================================================================
#ifdef EZMK_FILEWATCHER_MACOS

void FileWatcher::macos_worker() {
    struct kevent changes[32];
    struct kevent events[32];

    while (!stop_requested_) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 500 * 1000000; // 500ms

        int nev = kevent(kq_, nullptr, 0, events, 32, &ts);
        if (nev < 0) {
            if (errno == EINTR) continue;
            break;
        }

        bool got_event = false;
        for (int i = 0; i < nev; ++i) {
            if (events[i].filter == EVFILT_VNODE) {
                // Find which watch this belongs to
                for (auto& w : watches_) {
                    if (static_cast<uintptr_t>(events[i].ident) == static_cast<uintptr_t>(w.fd)) {
                        {
                            std::lock_guard<std::mutex> lock(pending_mutex_);
                            pending_paths_.insert(path_key(w.path));
                            last_event_ = std::chrono::steady_clock::now();
                        }
                        got_event = true;

                        // Re-register the event (kqueue EV_CLEAR semantics)
                        EV_SET(&changes[0], events[i].ident, EVFILT_VNODE,
                               EV_ADD | EV_CLEAR | EV_ENABLE,
                               NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND,
                               0, nullptr);
                        kevent(kq_, changes, 1, nullptr, 0, nullptr);
                        break;
                    }
                }
            }
        }
    }
}

void FileWatcher::macos_cleanup() {
    for (auto& w : watches_) {
        if (w.fd >= 0) close(w.fd);
    }
    watches_.clear();
    if (kq_ >= 0) {
        close(kq_);
        kq_ = -1;
    }
}

#endif // EZMK_FILEWATCHER_MACOS

// ===================================================================
// run() — platform dispatch
// ===================================================================

void FileWatcher::run() {
    if (dirs_.empty()) {
        util::warn("FileWatcher: no directories to watch");
        return;
    }

    running_ = true;
    stop_requested_ = false;

#ifdef EZMK_FILEWATCHER_WIN
    // Create IOCP
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!iocp_) {
        util::error("FileWatcher: failed to create IOCP");
        running_ = false;
        return;
    }

    // Add all watches
    for (auto& dir : dirs_) {
        win32_add_watch(dir);
    }

    if (watches_.empty()) {
        util::error("FileWatcher: no directories could be opened for watching");
        CloseHandle(iocp_);
        iocp_ = nullptr;
        running_ = false;
        return;
    }

    // Start worker thread
    worker_ = std::thread(&FileWatcher::win32_worker, this);

    // Main loop: handle debounce flushing
    while (!stop_requested_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check_and_flush();
    }

    // Signal IOCP to wake up worker
    PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);

    if (worker_.joinable()) worker_.join();
    win32_cleanup();
    running_ = false;

#elif defined(EZMK_FILEWATCHER_LINUX)
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ < 0) {
        util::error("FileWatcher: inotify_init failed");
        running_ = false;
        return;
    }

    for (auto& dir : dirs_) {
        if (!util::file_exists(dir)) {
            util::warn(std::string("watch directory not found, skipping: ") + dir.string());
            continue;
        }
        uint32_t mask = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MODIFY;
        int wd = inotify_add_watch(inotify_fd_, dir.string().c_str(), mask);
        if (wd < 0) {
            util::warn(std::string("failed to watch directory: ") + dir.string());
            continue;
        }
        wd_to_path_[wd] = dir.string();
    }

    if (wd_to_path_.empty()) {
        util::error("FileWatcher: no directories could be opened for watching");
        close(inotify_fd_);
        inotify_fd_ = -1;
        running_ = false;
        return;
    }

    worker_ = std::thread(&FileWatcher::linux_worker, this);

    // Main loop: handle debounce flushing
    while (!stop_requested_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check_and_flush();
    }

    if (worker_.joinable()) worker_.join();
    linux_cleanup();
    running_ = false;

#elif defined(EZMK_FILEWATCHER_MACOS)
    kq_ = kqueue();
    if (kq_ < 0) {
        util::error("FileWatcher: kqueue creation failed");
        running_ = false;
        return;
    }

    struct kevent changes[32];
    int change_idx = 0;

    for (auto& dir : dirs_) {
        if (!util::file_exists(dir)) {
            util::warn(std::string("watch directory not found, skipping: ") + dir.string());
            continue;
        }

        int fd = open(dir.string().c_str(), O_RDONLY);
        if (fd < 0) {
            util::warn(std::string("failed to open watch directory: ") + dir.string());
            continue;
        }

        EV_SET(&changes[change_idx++], fd, EVFILT_VNODE,
               EV_ADD | EV_CLEAR | EV_ENABLE,
               NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND,
               0, nullptr);

        KqWatch w;
        w.path = dir.string();
        w.fd = fd;
        watches_.push_back(w);
    }

    if (watches_.empty()) {
        util::error("FileWatcher: no directories could be opened for watching");
        close(kq_);
        kq_ = -1;
        running_ = false;
        return;
    }

    // Register all events
    if (kevent(kq_, changes, change_idx, nullptr, 0, nullptr) < 0) {
        util::error("FileWatcher: kevent registration failed");
        macos_cleanup();
        running_ = false;
        return;
    }

    worker_ = std::thread(&FileWatcher::macos_worker, this);

    // Main loop: handle debounce flushing
    while (!stop_requested_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check_and_flush();
    }

    if (worker_.joinable()) worker_.join();
    macos_cleanup();
    running_ = false;

#else
    util::error("FileWatcher: unsupported platform");
    running_ = false;
#endif

    // Final flush of any remaining pending paths
    flush_pending();
}

} // namespace ezmk::util
