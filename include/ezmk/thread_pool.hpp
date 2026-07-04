#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace ezmk::util {

// Simple thread pool for parallel compilation tasks.
// Fixed number of worker threads that pull from a shared task queue.
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads)
        : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    // Submit a callable and return a future for its result.
    template <typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using ResultType = decltype(f());
        auto promise = std::make_shared<std::promise<ResultType>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("ThreadPool is stopped")));
                return future;
            }
            tasks_.emplace([promise, fn = std::forward<F>(f)]() mutable {
                try {
                    if constexpr (std::is_void_v<ResultType>) {
                        fn();
                        promise->set_value();
                    } else {
                        promise->set_value(fn());
                    }
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        condition_.notify_one();
        return future;
    }

    size_t size() const { return workers_.size(); }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

} // namespace ezmk::util
