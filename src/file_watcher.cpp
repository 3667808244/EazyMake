#include "ezmk/util.hpp"
#include "ezmk/file_watcher.hpp"

#include <algorithm>
#include <cctype>
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

// 1.4.2 F-32: Windows path comparison is case-insensitive.
static std::string fold_case(std::string s) {
#ifdef EZMK_FILEWATCHER_WIN
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return s;
}

// ===================================================================
// Constructor / Destructor
// ===================================================================

FileWatcher::FileWatcher(Callback cb, int debounce_ms)
    : callback_(std::move(cb))
    , debounce_ms_(debounce_ms) {
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::add_directory(const fs::path& dir, bool recursive) {
    if (dir.empty()) {
        util::warn("FileWatcher: skipping empty directory path");
        return;
    }
    // 1.1.3 C4: 原 recursive_ 字段是死代码（三平台实现都不读），已删除。递归行为
    // 取决于平台实现（Windows bWatchSubtree=TRUE；Linux/macOS 仅监听目录本身）。
    // 参数保留以兼容 API，当前不生效——未来按平台实现真正的递归。
    (void)recursive;
    dirs_.push_back(fs::absolute(dir));
}

// 1.4.2 F-32: build-artifact filtering. A prefix matches whole directory trees
// (e.g. <root>/build/), a suffix matches extensions (e.g. ".o"). Both are
// normalized the same way events are (absolute generic string, folded case on
// Windows) so the comparison is stable across platforms.
void FileWatcher::add_ignore_prefix(const std::string& prefix) {
    if (prefix.empty()) return;
    fs::path p(prefix);
    if (p.is_relative()) p = fs::absolute(p);
    std::string s = fold_case(p.lexically_normal().generic_string());
    if (s.empty()) return;
    if (s.back() != '/') s.push_back('/');
    ignore_prefixes_.push_back(s);
}

void FileWatcher::add_ignore_suffix(const std::string& suffix) {
    if (suffix.empty()) return;
    ignore_suffixes_.push_back(fold_case(suffix));
}

bool FileWatcher::is_ignored(const std::string& abs_generic) const {
    if (abs_generic.empty()) return false;
    std::string s = fold_case(abs_generic);
    for (const auto& p : ignore_prefixes_) {
        if (p.empty()) continue;
        if (s.size() >= p.size() && s.compare(0, p.size(), p) == 0) {
            return true;  // under the ignored directory
        }
        // The ignored directory ITSELF (event for "…/build" vs rule "…/build/").
        if (s.size() + 1 == p.size() && p.compare(0, s.size(), s) == 0) {
            return true;
        }
    }
    for (const auto& suf : ignore_suffixes_) {
        if (!suf.empty() && s.size() >= suf.size() &&
            s.compare(s.size() - suf.size(), suf.size(), suf) == 0) {
            return true;
        }
    }
    return false;
}

// 1.4.2 F-32: single entry point for platform events — ignore-filtered insert
// into the debounce set (used by all three platform workers).
void FileWatcher::note_event(const fs::path& p) {
    std::string key = path_key(p);
    if (is_ignored(key)) return;
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_paths_.insert(std::move(key));
    last_event_ = std::chrono::steady_clock::now();
}

// 1.4.2 F-05: record a platform-worker hard error so run() can exit.
void FileWatcher::report_worker_error(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(worker_error_mutex_);
        worker_error_msg_ = msg;
    }
    worker_error_.store(true);
}

std::string FileWatcher::worker_error_message() const {
    std::lock_guard<std::mutex> lock(worker_error_mutex_);
    return worker_error_msg_;
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

    // Filter: only keep files that actually exist (ignore transient editor temp
    // files) and fire callback for each unique path.
    // 1.4.2 F-32: the existence check is what the pre-1.4.2 comment claimed but
    // never did — a delete event must not trigger a rebuild (the file is gone),
    // and it also drops paths that were removed before the debounce expired.
    for (auto& p : paths) {
        fs::path fp(p);
        // Skip editor temp/swap files
        auto ext = fp.extension().string();
        auto name = fp.filename().string();
        if (!name.empty() && name[0] == '.') continue;  // hidden files
        // 1.1.3 C5: 根目录（如 C:\）上 fp.filename() 可能为空，name.back() 是 UB
        if (!name.empty() && name.back() == '~') continue;  // vim backup files
        if (ext == ".swp" || ext == ".swx" || ext == ".tmp") continue;

        // 1.4.2 F-32: ignore rules are also applied here (a path could have
        // entered pending before a rule existed, and platform-side filtering is
        // best-effort on macOS which reports directories).
        if (is_ignored(p)) continue;

        std::error_code ec;
        if (!fs::exists(fp, ec) || ec) continue;  // 1.4.2 F-32: deletion → no rebuild

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

// 1.4.2: shared run() main loop — debounce flushing, worker-death detection
// (F-05) and the low-frequency watch-repair pass (F-31). Returns when stop()
// was called or the platform worker died.
void FileWatcher::main_loop() {
    auto next_repair = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!stop_requested_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check_and_flush();

        if (worker_error_.load()) {
            util::warn(std::string("FileWatcher: worker stopped: ") + worker_error_message());
            break;
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= next_repair) {
            next_repair = now + std::chrono::seconds(2);
#ifdef EZMK_FILEWATCHER_WIN
            win32_repair_watches();
#elif defined(EZMK_FILEWATCHER_LINUX)
            linux_repair_watches();
#elif defined(EZMK_FILEWATCHER_MACOS)
            macos_repair_watches();
#endif
        }
    }
}

// ===================================================================
// Windows implementation (ReadDirectoryChangesW + IOCP)
// ===================================================================
#ifdef EZMK_FILEWATCHER_WIN

// Post one ReadDirectoryChangesW on an existing watch entry. Caller must hold
// platform_mutex_ (or be the single-threaded startup path).
bool FileWatcher::win32_arm(WatchEntry& w) {
    OVERLAPPED* ov = static_cast<OVERLAPPED*>(w.overlapped);
    memset(ov, 0, sizeof(OVERLAPPED));  // reused across re-arms — must be clean
    DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                   FILE_NOTIFY_CHANGE_DIR_NAME |
                   FILE_NOTIFY_CHANGE_LAST_WRITE |
                   FILE_NOTIFY_CHANGE_SIZE;
    if (!ReadDirectoryChangesW(
            static_cast<HANDLE>(w.dir_handle),
            w.buffer.data(),
            static_cast<DWORD>(w.buffer.size()),
            TRUE,  // watch subtree
            filter,
            nullptr,
            ov,
            nullptr)) {
        w.armed = false;
        return false;
    }
    w.armed = true;
    return true;
}

size_t FileWatcher::win32_index_of_overlapped(void* ov) const {
    for (size_t i = 0; i < watches_.size(); ++i) {
        if (watches_[i].overlapped == ov) return i;
    }
    return watches_.size();  // not found
}

bool FileWatcher::win32_add_watch(const fs::path& dir) {
    if (!util::file_exists(dir)) {
        util::warn(std::string("watch directory not found, skipping: ") + dir.string());
        return false;
    }

    // Open directory handle for overlapped I/O
    // NOTE: 1.4.2 F-20 (阶段五) replaces this narrow→wide byte copy with the
    // shared CP_UTF8 conversion layer; kept as-is here.
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
        return false;
    }

    // The completion key is the watch index — stable for the entry's lifetime
    // (repair re-opens the handle but keeps the same key).
    size_t index = watches_.size();
    if (CreateIoCompletionPort(hDir, static_cast<HANDLE>(iocp_), (ULONG_PTR)index, 0) == nullptr) {
        CloseHandle(hDir);
        util::warn(std::string("failed to associate directory with IOCP: ") + dir_str);
        return false;
    }

    WatchEntry entry;
    entry.dir_path = dir_str;
    entry.dir_handle = hDir;
    entry.buffer.resize(64 * 1024); // 64KB buffer

    // 1.1.3 C3: OVERLAPPED 生命周期归本实例（overlapped_pool_），不再共享文件级全局池
    auto* ov = new OVERLAPPED();
    memset(ov, 0, sizeof(OVERLAPPED));
    entry.overlapped = ov;
    overlapped_pool_.push_back(ov);

    watches_.push_back(std::move(entry));
    return true;
}

void FileWatcher::win32_worker() {
    // Post initial ReadDirectoryChangesW for each watch
    {
        std::lock_guard<std::mutex> lock(platform_mutex_);
        for (size_t i = 0; i < watches_.size(); ++i) {
            auto& w = watches_[i];
            if (!win32_arm(w)) {
                util::warn(std::string("FileWatcher: initial watch failed for ") +
                           w.dir_path + " (error " + std::to_string(GetLastError()) + ")");
            }
        }
    }

    // Event loop
    while (!stop_requested_) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* ov = nullptr;

        // Wait up to 500ms for IOCP events
        BOOL ok = GetQueuedCompletionStatus(
            static_cast<HANDLE>(iocp_),
            &bytesTransferred,
            &completionKey,
            &ov,
            500  // timeout ms
        );

        if (!ok) {
            DWORD err = GetLastError();
            if (ov == nullptr && err == WAIT_TIMEOUT) {
                // Timeout — debounce flushing happens in run()'s main loop.
                continue;
            }
            if (ov != nullptr) {
                // 1.4.2 F-31: a read failed — the watch died (e.g. the
                // directory was removed). Drop the armed mark; run()'s repair
                // pass re-opens the handle once the directory is back.
                std::lock_guard<std::mutex> lock(platform_mutex_);
                size_t idx = win32_index_of_overlapped(ov);
                if (idx < watches_.size()) watches_[idx].armed = false;
                continue;
            }
            // 1.2.0-dev.11: a real IOCP error previously fell into the same
            // branch as a timeout and the loop spun forever. Fail loudly and
            // let run() observe the death (1.4.2 F-05) instead of hanging.
            report_worker_error("IOCP error " + std::to_string(err));
            break;
        }

        if (ov == nullptr) {
            // Shutdown sentinel (PostQueuedCompletionStatus) — see stop handling.
            continue;
        }

        std::lock_guard<std::mutex> lock(platform_mutex_);
        size_t idx = static_cast<size_t>(completionKey);
        if (idx >= watches_.size()) continue;
        auto& w = watches_[idx];

        if (bytesTransferred == 0) {
            // Cancelled/closed read — the directory is gone; repair will re-arm.
            w.armed = false;
            continue;
        }

        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(w.buffer.data());
        while (true) {
            // Convert wide-char filename to UTF-8 path
            std::wstring wfname(info->FileName, info->FileNameLength / sizeof(WCHAR));
            std::string fname(wfname.begin(), wfname.end());
            note_event(fs::path(w.dir_path) / fname);

            if (info->NextEntryOffset == 0) break;
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
        }

        // Re-post the read
        if (!win32_arm(w)) {
            // 1.4.2 F-31: warn once per loss (no error spam), retry via repair.
            if (!w.lost_warned) {
                w.lost_warned = true;
                util::warn(std::string("FileWatcher: read re-arm failed for ") +
                           w.dir_path + " (error " + std::to_string(GetLastError()) +
                           ") — will retry");
            }
        }
    }
}

// 1.4.2 F-31: re-open watches whose directory disappeared and came back.
// Runs on run()'s thread; must hold platform_mutex_ (the worker arms reads
// under the same lock, so no read can be in flight for an unarmed entry).
void FileWatcher::win32_repair_watches() {
    std::lock_guard<std::mutex> lock(platform_mutex_);
    for (size_t i = 0; i < watches_.size(); ++i) {
        auto& w = watches_[i];
        if (w.armed) continue;

        fs::path dir(w.dir_path);
        if (!util::file_exists(dir)) continue;  // still gone — keep waiting

        if (w.dir_handle) {
            CancelIo(static_cast<HANDLE>(w.dir_handle));
            CloseHandle(static_cast<HANDLE>(w.dir_handle));
            w.dir_handle = nullptr;
        }

        std::wstring wdir(w.dir_path.begin(), w.dir_path.end());
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
            if (!w.lost_warned) {
                w.lost_warned = true;
                util::warn(std::string("FileWatcher: watch lost for ") + w.dir_path +
                           " (error " + std::to_string(GetLastError()) + ") — will retry");
            }
            continue;
        }
        if (CreateIoCompletionPort(hDir, static_cast<HANDLE>(iocp_), (ULONG_PTR)i, 0) == nullptr) {
            CloseHandle(hDir);
            if (!w.lost_warned) {
                w.lost_warned = true;
                util::warn(std::string("FileWatcher: watch lost for ") + w.dir_path +
                           " (IOCP re-associate failed) — will retry");
            }
            continue;
        }

        w.dir_handle = hDir;
        if (win32_arm(w)) {
            if (w.lost_warned) {
                util::info(std::string("FileWatcher: re-established watch for ") + w.dir_path);
            }
            w.lost_warned = false;
            w.repaired = true;
        } else if (!w.lost_warned) {
            w.lost_warned = true;
            util::warn(std::string("FileWatcher: watch lost for ") + w.dir_path +
                       " (error " + std::to_string(GetLastError()) + ") — will retry");
        }
    }
}

void FileWatcher::win32_cleanup() {
    for (auto& w : watches_) {
        if (w.dir_handle) {
            CancelIo(static_cast<HANDLE>(w.dir_handle));
            CloseHandle(static_cast<HANDLE>(w.dir_handle));
        }
    }
    watches_.clear();
    // 1.1.3 C3: 释放本实例持有的 OVERLAPPED（仅清理自己的，不影响其他实例）
    for (void* p : overlapped_pool_) delete static_cast<OVERLAPPED*>(p);
    overlapped_pool_.clear();
    if (iocp_) {
        CloseHandle(static_cast<HANDLE>(iocp_));
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
            report_worker_error("inotify poll failed (errno " + std::to_string(errno) + ")");
            break;
        }
        if (ret == 0) {
            // Timeout — check debounce
            continue;
        }

        ssize_t len = read(inotify_fd_, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR) continue;
            report_worker_error("inotify read failed (errno " + std::to_string(errno) + ")");
            break;
        }

        for (char* ptr = buf; ptr < buf + len; ) {
            auto* event = reinterpret_cast<struct inotify_event*>(ptr);

            // 1.4.2 F-31: the kernel dropped this watch (directory removed or
            // moved away, or the watch was explicitly removed). Erase the wd
            // mapping so a reused descriptor can never resolve to the stale
            // path; run()'s repair pass re-adds the watch when the directory
            // returns.
            if (event->mask & IN_IGNORED) {
                std::lock_guard<std::mutex> lock(platform_mutex_);
                wd_to_path_.erase(event->wd);
                ptr += sizeof(struct inotify_event) + event->len;
                continue;
            }

            if (event->len > 0) {
                std::string dir;
                {
                    std::lock_guard<std::mutex> lock(platform_mutex_);
                    auto it = wd_to_path_.find(event->wd);
                    if (it != wd_to_path_.end()) dir = it->second;
                }
                if (!dir.empty()) {
                    std::string fname = event->name;
                    // Skip editor temp files
                    if (!fname.empty() && (fname[0] == '.' || fname.back() == '~')) {
                        ptr += sizeof(struct inotify_event) + event->len;
                        continue;
                    }
                    note_event(fs::path(dir) / fname);
                }
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
}

// 1.4.2 F-31: re-add watches whose directory disappeared and came back.
void FileWatcher::linux_repair_watches() {
    std::lock_guard<std::mutex> lock(platform_mutex_);
    for (auto& dir : dirs_) {
        std::string path = dir.string();

        bool watched = false;
        for (const auto& [wd, p] : wd_to_path_) {
            (void)wd;
            if (p == path) { watched = true; break; }
        }
        if (watched) {
            repair_warned_.erase(path);
            continue;
        }
        if (!util::file_exists(dir)) continue;  // still gone — keep waiting

        uint32_t mask = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MODIFY;
        int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
        if (wd < 0) {
            // 1.4.2 F-31: warn once per loss instead of spamming every 2s.
            if (repair_warned_.insert(path).second) {
                util::warn(std::string("FileWatcher: watch lost for ") + path +
                           " (errno " + std::to_string(errno) + ") — will retry");
            }
            continue;
        }
        wd_to_path_[wd] = path;
        bool was_warned = repair_warned_.erase(path) > 0;
        if (was_warned) {
            util::info(std::string("FileWatcher: re-established watch for ") + path);
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

bool FileWatcher::macos_register(KqWatch& w) {
    struct kevent ch;
    EV_SET(&ch, static_cast<uintptr_t>(w.fd), EVFILT_VNODE,
           EV_ADD | EV_CLEAR | EV_ENABLE,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND,
           0, nullptr);
    return kevent(kq_, &ch, 1, nullptr, 0, nullptr) == 0;
}

void FileWatcher::macos_worker() {
    // kqueue EVFILT_VNODE semantics (1.2.0-dev.11):
    //  - The watch is on the directory vnode itself, NOT on its entries, and
    //    NOT recursive. Only the directory's own NOTE_WRITE/NOTE_DELETE/
    //    NOTE_RENAME/NOTE_EXTEND arrive — per-file events are NOT delivered.
    //  - Consequently the callback receives the watched directory path
    //    (w.path), never a specific changed file — unlike Windows/Linux.
    //    FSEvents-based per-file reporting is a separate, deferred work item.
    //  - 1.4.2 F-31: if the directory is deleted or renamed away the vnode goes
    //    stale; the watch is marked lost and run()'s repair pass reopens the
    //    (recreated) directory. (The watch still does not FOLLOW a rename.)
    //  - 1.4.2 F-36: the event arrays are sized from the watch count instead of
    //    a fixed 32 slots (which truncated events and overflowed `changes`).
    size_t n = 1;
    {
        std::lock_guard<std::mutex> lock(platform_mutex_);
        if (!watches_.empty()) n = watches_.size();
    }
    std::vector<struct kevent> events(n);

    while (!stop_requested_) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 500 * 1000000; // 500ms

        int nev = kevent(kq_, nullptr, 0, events.data(),
                         static_cast<int>(events.size()), &ts);
        if (nev < 0) {
            if (errno == EINTR) continue;
            report_worker_error("kevent wait failed (errno " + std::to_string(errno) + ")");
            break;
        }

        for (int i = 0; i < nev; ++i) {
            if (events[i].filter != EVFILT_VNODE) continue;

            std::lock_guard<std::mutex> lock(platform_mutex_);
            for (auto& w : watches_) {
                if (static_cast<uintptr_t>(events[i].ident) !=
                    static_cast<uintptr_t>(w.fd)) {
                    continue;
                }
                if (events[i].fflags & (NOTE_DELETE | NOTE_RENAME)) {
                    // vnode gone — stop reporting it; repair reopens if the
                    // directory comes back.
                    w.lost = true;
                    break;
                }
                note_event(fs::path(w.path));
                if (!macos_register(w)) {
                    w.lost = true;
                    util::warn(std::string("FileWatcher: kevent re-register failed for ") +
                               w.path + " (errno " + std::to_string(errno) + ")");
                }
                break;
            }
        }
    }
}

// 1.4.2 F-31: reopen watches whose directory disappeared and came back.
void FileWatcher::macos_repair_watches() {
    std::lock_guard<std::mutex> lock(platform_mutex_);
    for (auto& w : watches_) {
        if (!w.lost) continue;
        fs::path dir(w.path);
        if (!util::file_exists(dir)) continue;  // still gone — keep waiting

        if (w.fd >= 0) close(w.fd);
        int fd = open(w.path.c_str(), O_RDONLY);
        if (fd < 0) {
            if (repair_warned_.insert(w.path).second) {
                util::warn(std::string("FileWatcher: watch lost for ") + w.path +
                           " (errno " + std::to_string(errno) + ") — will retry");
            }
            continue;
        }
        w.fd = fd;
        if (!macos_register(w)) {
            if (repair_warned_.insert(w.path).second) {
                util::warn(std::string("FileWatcher: watch lost for ") + w.path +
                           " (kevent register failed) — will retry");
            }
            close(w.fd);
            w.fd = -1;
            continue;
        }
        bool was_warned = repair_warned_.erase(w.path) > 0;
        w.lost = false;
        if (was_warned) {
            util::info(std::string("FileWatcher: re-established watch for ") + w.path);
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
    worker_error_ = false;
    {
        std::lock_guard<std::mutex> lock(worker_error_mutex_);
        worker_error_msg_.clear();
    }

#ifdef EZMK_FILEWATCHER_WIN
    // Create IOCP
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!iocp_) {
        util::error("FileWatcher: failed to create IOCP");
        running_ = false;
        return;
    }

    // Add all watches (each entry keeps its own handle/buffer/OVERLAPPED; the
    // worker posts the initial reads).
    for (auto& dir : dirs_) {
        win32_add_watch(dir);
    }

    if (watches_.empty()) {
        util::error("FileWatcher: no directories could be opened for watching");
        CloseHandle(static_cast<HANDLE>(iocp_));
        iocp_ = nullptr;
        running_ = false;
        return;
    }

    // Start worker thread
    worker_ = std::thread(&FileWatcher::win32_worker, this);

    // Main loop: debounce flushing + worker-death detection + watch repair
    main_loop();

    // Signal IOCP to wake up worker
    PostQueuedCompletionStatus(static_cast<HANDLE>(iocp_), 0, 0, nullptr);

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

    // Main loop: debounce flushing + worker-death detection + watch repair
    main_loop();

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

    // 1.4.2 F-36: dynamic registration array (was a fixed 32 slots).
    std::vector<struct kevent> changes;
    changes.reserve(dirs_.size());

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

        KqWatch w;
        w.path = dir.string();
        w.fd = fd;
        watches_.push_back(w);

        struct kevent ch;
        EV_SET(&ch, static_cast<uintptr_t>(fd), EVFILT_VNODE,
               EV_ADD | EV_CLEAR | EV_ENABLE,
               NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND,
               0, nullptr);
        changes.push_back(ch);
    }

    if (watches_.empty()) {
        util::error("FileWatcher: no directories could be opened for watching");
        close(kq_);
        kq_ = -1;
        running_ = false;
        return;
    }

    // Register all events
    if (kevent(kq_, changes.data(), static_cast<int>(changes.size()),
               nullptr, 0, nullptr) < 0) {
        util::error("FileWatcher: kevent registration failed");
        macos_cleanup();
        running_ = false;
        return;
    }

    worker_ = std::thread(&FileWatcher::macos_worker, this);

    // Main loop: debounce flushing + worker-death detection + watch repair
    main_loop();

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
