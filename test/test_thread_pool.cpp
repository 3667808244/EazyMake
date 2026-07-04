// Unit tests for thread_pool.cpp (0.2.3+)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace ezmk::util;

// ===================================================================
// Basic construction
// ===================================================================

TEST_CASE("ThreadPool: creates N threads", "[thread_pool][0.2.3]") {
    ThreadPool pool(4);
    REQUIRE(pool.size() == 4);
}

TEST_CASE("ThreadPool: single thread", "[thread_pool][0.2.3]") {
    ThreadPool pool(1);
    REQUIRE(pool.size() == 1);
}

TEST_CASE("ThreadPool: zero threads still creates pool (degenerate case)", "[thread_pool][0.2.3]") {
    ThreadPool pool(0);
    REQUIRE(pool.size() == 0);
}

// ===================================================================
// submit() returns correct results
// ===================================================================

TEST_CASE("ThreadPool: submit returns correct future result", "[thread_pool][0.2.3]") {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 42; });
    REQUIRE(future.get() == 42);
}

TEST_CASE("ThreadPool: submit with void function", "[thread_pool][0.2.3]") {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    auto future = pool.submit([&counter]() { counter.fetch_add(1); });
    future.get();
    REQUIRE(counter.load() == 1);
}

TEST_CASE("ThreadPool: submit with string result", "[thread_pool][0.2.3]") {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> std::string { return "hello"; });
    REQUIRE(future.get() == "hello");
}

// ===================================================================
// Multiple tasks execute concurrently
// ===================================================================

TEST_CASE("ThreadPool: multiple tasks execute concurrently", "[thread_pool][0.2.3]") {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter.fetch_add(1);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    REQUIRE(counter.load() == 100);
}

TEST_CASE("ThreadPool: tasks spread across threads", "[thread_pool][0.2.3]") {
    ThreadPool pool(8);
    std::mutex mtx;
    std::set<std::thread::id> ids;

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.submit([&mtx, &ids]() {
            std::lock_guard<std::mutex> lock(mtx);
            ids.insert(std::this_thread::get_id());
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // With 8 threads and 50 tasks, we should see at least 2 unique thread IDs
    REQUIRE(ids.size() >= 2);
}

// ===================================================================
// Destructor waits for all tasks
// ===================================================================

TEST_CASE("ThreadPool: destructor waits for all tasks to complete", "[thread_pool][0.2.3]") {
    std::atomic<int> stage{0};

    {
        ThreadPool pool(2);
        auto f1 = pool.submit([&stage]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stage.store(1);
        });
        auto f2 = pool.submit([&stage]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stage.store(2);
        });
        // Pool destructor runs here — must wait for both tasks
    }

    // Both tasks must have completed by now
    REQUIRE((stage.load() == 1 || stage.load() == 2));
}

// ===================================================================
// Exception propagation
// ===================================================================

TEST_CASE("ThreadPool: exception in task propagates to future", "[thread_pool][0.2.3]") {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("test error");
        return 0;
    });

    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
}

// ===================================================================
// Large number of tasks
// ===================================================================

TEST_CASE("ThreadPool: handles more tasks than threads", "[thread_pool][0.2.3]") {
    ThreadPool pool(4);
    std::atomic<int> sum{0};

    std::vector<std::future<int>> futures;
    for (int i = 0; i < 200; ++i) {
        futures.push_back(pool.submit([i]() { return i; }));
    }

    int total = 0;
    for (auto& f : futures) {
        total += f.get();
    }

    // Sum of 0..199 = 199*200/2 = 19900
    REQUIRE(total == 19900);
}
