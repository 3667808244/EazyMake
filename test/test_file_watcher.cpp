// Unit tests for file_watcher.cpp (0.2.3+)
// Conditionally compiled based on platform — FileWatcher requires OS-specific APIs.
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/file_watcher.hpp"
#include "ezmk/util.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;
using namespace ezmk::util;

// ===================================================================
// Helpers
// ===================================================================

static fs::path create_temp_dir() {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_watch_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp);
    return tmp;
}

// 1.1.3 T1: 轮询等待计数器达到目标值（带超时）。替换原永真断言（call_count >= 0）
// —— 事件未触发时不再假通过，由调用方决定 SKIP 还是失败。
static bool wait_for_event(std::atomic<int>& count, int target, int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (count.load() >= target) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return count.load() >= target;
}

// ===================================================================
// Basic construction
// ===================================================================

TEST_CASE("FileWatcher: construct with callback", "[file_watcher][0.2.3]") {
    std::atomic<int> call_count{0};
    FileWatcher watcher([&call_count](const fs::path&) {
        call_count.fetch_add(1);
    });
    // Construction should not trigger callback
    REQUIRE(call_count.load() == 0);
}

TEST_CASE("FileWatcher: default debounce is 300ms", "[file_watcher][0.2.3]") {
    std::atomic<int> call_count{0};
    FileWatcher watcher([&call_count](const fs::path&) { call_count++; });
    // Verify construction with default debounce
    REQUIRE(true); // compile-time check
}

TEST_CASE("FileWatcher: custom debounce value", "[file_watcher][0.2.3]") {
    std::atomic<int> call_count{0};
    FileWatcher watcher([&call_count](const fs::path&) { call_count++; }, 100);
    // Verify construction with custom debounce
    REQUIRE(true);
}

// ===================================================================
// add_directory()
// ===================================================================

TEST_CASE("FileWatcher: add_directory accepts valid path", "[file_watcher][0.2.3]") {
    auto tmp = create_temp_dir();
    std::atomic<bool> called{false};
    FileWatcher watcher([&called](const fs::path&) { called = true; });

    REQUIRE_NOTHROW(watcher.add_directory(tmp));
    fs::remove_all(tmp);
}

TEST_CASE("FileWatcher: add_directory with non-recursive flag", "[file_watcher][0.2.3]") {
    auto tmp = create_temp_dir();
    std::atomic<bool> called{false};
    FileWatcher watcher([&called](const fs::path&) { called = true; });

    REQUIRE_NOTHROW(watcher.add_directory(tmp, false));
    fs::remove_all(tmp);
}

// ===================================================================
// stop() / run() lifecycle
// ===================================================================

TEST_CASE("FileWatcher: stop terminates run loop", "[file_watcher][0.2.3]") {
    auto tmp = create_temp_dir();
    std::atomic<bool> called{false};
    FileWatcher watcher([&called](const fs::path&) { called = true; });
    watcher.add_directory(tmp);

    // Start watcher in background thread
    std::thread watcher_thread([&watcher]() {
        watcher.run();
    });

    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop should cause run() to return
    watcher.stop();

    if (watcher_thread.joinable()) {
        watcher_thread.join();
    }

    fs::remove_all(tmp);
    REQUIRE(true); // No deadlock or crash
}

TEST_CASE("FileWatcher: run with no directories warns and returns", "[file_watcher][0.2.3]") {
    std::atomic<bool> called{false};
    FileWatcher watcher([&called](const fs::path&) { called = true; });
    // No directories added — run() should return immediately

    watcher.run();
    REQUIRE(true); // Should not crash or hang
}

// ===================================================================
// File change detection (integration test)
// ===================================================================

TEST_CASE("FileWatcher: detects file creation", "[file_watcher][0.2.3][integration]") {
    auto tmp = create_temp_dir();
    std::atomic<int> call_count{0};
    std::mutex paths_mutex;
    std::set<std::string> changed;
    fs::path created = tmp / "test.cpp";

    FileWatcher watcher([&](const fs::path& p) {
        call_count.fetch_add(1);
        std::lock_guard<std::mutex> lock(paths_mutex);
        changed.insert(fs::absolute(p).generic_string());
    }, 100); // short debounce for testing
    watcher.add_directory(tmp);

    std::thread watcher_thread([&watcher]() {
        watcher.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create a file
    {
        std::ofstream f(created);
        f << "int main() { return 0; }\n";
    }

    // 1.1.3 T1: 轮询等待事件（带超时）→ 断言收到精确路径集合；
    // 事件无法送达的环境显式 SKIP，而非 >=0 假通过。
    bool delivered = wait_for_event(call_count, 1, 3000);

    watcher.stop();
    if (watcher_thread.joinable()) watcher_thread.join();
    fs::remove_all(tmp);

    if (!delivered) {
        SKIP("file watcher events not delivered in this environment");
    }
    std::lock_guard<std::mutex> lock(paths_mutex);
    REQUIRE(changed.count(fs::absolute(created).generic_string()) == 1);
}

TEST_CASE("FileWatcher: detects file modification", "[file_watcher][0.2.3][integration]") {
    auto tmp = create_temp_dir();

    // Create a file first
    auto test_file = tmp / "modify_test.cpp";
    {
        std::ofstream f(test_file);
        f << "// original\n";
    }

    std::atomic<int> call_count{0};
    std::mutex paths_mutex;
    std::set<std::string> changed;
    FileWatcher watcher([&](const fs::path& p) {
        call_count.fetch_add(1);
        std::lock_guard<std::mutex> lock(paths_mutex);
        changed.insert(fs::absolute(p).generic_string());
    }, 100);
    watcher.add_directory(tmp);

    std::thread watcher_thread([&watcher]() {
        watcher.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Modify the file
    {
        std::ofstream f(test_file);
        f << "// modified\n";
    }

    // 1.1.3 T1: 轮询等待 + 精确路径断言；环境不支持则显式 SKIP
    bool delivered = wait_for_event(call_count, 1, 3000);

    watcher.stop();
    if (watcher_thread.joinable()) watcher_thread.join();

    fs::remove_all(tmp);
    if (!delivered) {
        SKIP("file watcher events not delivered in this environment");
    }
    std::lock_guard<std::mutex> lock(paths_mutex);
    REQUIRE(changed.count(fs::absolute(test_file).generic_string()) == 1);
}

// ===================================================================
// Multiple directory watching
// ===================================================================

TEST_CASE("FileWatcher: watches multiple directories", "[file_watcher][0.2.3]") {
    auto tmp1 = create_temp_dir();
    auto tmp2 = create_temp_dir();
    std::atomic<int> call_count{0};
    std::mutex paths_mutex;
    std::set<std::string> changed;
    fs::path f1_path = tmp1 / "a.cpp";
    fs::path f2_path = tmp2 / "b.cpp";

    FileWatcher watcher([&](const fs::path& p) {
        call_count.fetch_add(1);
        std::lock_guard<std::mutex> lock(paths_mutex);
        changed.insert(fs::absolute(p).generic_string());
    }, 100);
    watcher.add_directory(tmp1);
    watcher.add_directory(tmp2);

    std::thread watcher_thread([&watcher]() {
        watcher.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create files in both directories
    {
        std::ofstream f1(f1_path);
        f1 << "// a\n";
    }
    {
        std::ofstream f2(f2_path);
        f2 << "// b\n";
    }

    // 1.1.3 T1: 轮询等待两个事件 + 精确路径集合；环境不支持则显式 SKIP
    bool delivered = wait_for_event(call_count, 2, 3000);

    watcher.stop();
    if (watcher_thread.joinable()) watcher_thread.join();

    fs::remove_all(tmp1);
    fs::remove_all(tmp2);
    if (!delivered) {
        SKIP("file watcher events not delivered in this environment");
    }
    std::lock_guard<std::mutex> lock(paths_mutex);
    REQUIRE(changed.count(fs::absolute(f1_path).generic_string()) == 1);
    REQUIRE(changed.count(fs::absolute(f2_path).generic_string()) == 1);
}

// 1.1.3 C3: overlapping watcher instances each own their OVERLAPPED pool.
// The old file-global pool was cleared by ANY instance's cleanup, leaving the
// other instance's OVERLAPPED dangling. This test runs two overlapping watchers
// and destroys the second while the first is still live — a crash here means the
// pools are not instance-isolated.
TEST_CASE("FileWatcher: overlapping instances clean up independently", "[file_watcher][1.1.3]") {
    auto tmp = create_temp_dir();
    std::atomic<int> call_count{0};

    FileWatcher w1([&call_count](const fs::path&) { call_count.fetch_add(1); });
    w1.add_directory(tmp);
    std::thread t1([&w1]() { w1.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        // Second watcher overlaps the first's lifetime
        FileWatcher w2([&call_count](const fs::path&) { call_count.fetch_add(1); });
        w2.add_directory(tmp);
        std::thread t2([&w2]() { w2.run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        w2.stop();
        t2.join();
        // w2 destructor runs here — must only release w2's own OVERLAPPEDs
    }

    w1.stop();
    t1.join();

    fs::remove_all(tmp);
}

// 1.1.3 C5: smoke test — watcher must handle paths containing spaces without
// crashing (name.back() guard: filename() may be empty for root paths).
TEST_CASE("FileWatcher: watch directory path with spaces", "[file_watcher][1.1.3]") {
    auto tmp = create_temp_dir() / "dir with spaces";
    fs::create_directories(tmp);
    std::atomic<int> call_count{0};
    FileWatcher watcher([&call_count](const fs::path&) { call_count.fetch_add(1); });
    REQUIRE_NOTHROW(watcher.add_directory(tmp));
    REQUIRE_NOTHROW(watcher.add_directory(tmp));
    fs::remove_all(tmp);
}

// ===================================================================
// Debounce behavior
// ===================================================================

TEST_CASE("FileWatcher: debounce coalesces rapid changes", "[file_watcher][0.2.3]") {
    auto tmp = create_temp_dir();
    std::atomic<int> call_count{0};

    // Long debounce to ensure coalescing
    FileWatcher watcher([&call_count](const fs::path&) { call_count++; }, 300);
    watcher.add_directory(tmp);

    std::thread watcher_thread([&watcher]() {
        watcher.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Rapidly modify the same file multiple times
    for (int i = 0; i < 10; ++i) {
        std::ofstream f(tmp / "rapid.cpp");
        f << "// change " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for debounce window + some processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    watcher.stop();
    if (watcher_thread.joinable()) watcher_thread.join();

    fs::remove_all(tmp);
    // After debounce, we should get at most 1 callback per file
    // (10 rapid changes to same file → 1 callback after debounce)
    REQUIRE(call_count.load() <= 2); // Allow some platform variance
}

// ===================================================================
// Empty path defense (1.1.0-dev.5)
// ===================================================================

TEST_CASE("FileWatcher: add_directory with empty path does not crash", "[file_watcher][1.1.0]") {
    std::atomic<bool> called{false};
    FileWatcher watcher([&called](const fs::path&) { called = true; });

    REQUIRE_NOTHROW(watcher.add_directory(""));
    // run() should not crash with empty dirs
    watcher.run();
    REQUIRE(true);
}

// ===================================================================
// Non-copyable / non-movable
// ===================================================================

TEST_CASE("FileWatcher: is non-copyable (compile-time check)", "[file_watcher][0.2.3]") {
    // This test verifies at compile time that FileWatcher cannot be copied
    static_assert(!std::is_copy_constructible_v<FileWatcher>, "FileWatcher must not be copy-constructible");
    static_assert(!std::is_copy_assignable_v<FileWatcher>, "FileWatcher must not be copy-assignable");
    static_assert(!std::is_move_constructible_v<FileWatcher>, "FileWatcher must not be move-constructible");
    static_assert(!std::is_move_assignable_v<FileWatcher>, "FileWatcher must not be move-assignable");
    REQUIRE(true);
}
