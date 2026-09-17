#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace cyka {

inline constexpr unsigned DEFAULT_HW_THREADS = 4;

namespace detail {

inline std::optional<unsigned>& parallelThreadOverride() {
    static std::optional<unsigned> threads;
    return threads;
}

/// Process-wide worker pool — avoids spawn/join per `parallelFor`.
class ThreadPool {
  public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void parallelFor(std::size_t count, const std::function<void(std::size_t)>& callback) {
        if (count == 0) {
            return;
        }
        ensureWorkers();
        const unsigned WORKERS = static_cast<unsigned>(
            std::min<std::size_t>(workers.empty() ? 1 : workers.size(), count));
        if (WORKERS <= 1 || workers.empty()) {
            for (std::size_t idx = 0; idx < count; ++idx) {
                callback(idx);
            }
            return;
        }

        std::atomic<std::size_t> next{0};
        std::atomic<std::size_t> done{0};
        std::mutex done_mu;
        std::condition_variable done_cv;

        auto body = [&] {
            for (;;) {
                const std::size_t INDEX = next.fetch_add(1, std::memory_order_relaxed);
                if (INDEX >= count) {
                    break;
                }
                callback(INDEX);
            }
            if (done.fetch_add(1, std::memory_order_acq_rel) + 1 == WORKERS) {
                const std::scoped_lock LOCK(done_mu);
                done_cv.notify_one();
            }
        };

        {
            const std::scoped_lock LOCK(queue_mu);
            for (unsigned worker = 0; worker < WORKERS; ++worker) {
                queue.emplace(body);
            }
        }
        queue_cv.notify_all();

        std::unique_lock lock(done_mu);
        done_cv.wait(lock, [&] { return done.load(std::memory_order_acquire) >= WORKERS; });
    }

  private:
    ThreadPool() = default;
    ~ThreadPool() {
        {
            const std::scoped_lock LOCK(queue_mu);
            stopping = true;
        }
        queue_cv.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ensureWorkers() {
        const unsigned WANT = threadBudgetUnlocked();
        const std::scoped_lock LOCK(queue_mu);
        if (workers.size() >= WANT || WANT == 0) {
            return;
        }
        const unsigned ADD = WANT - static_cast<unsigned>(workers.size());
        for (unsigned idx = 0; idx < ADD; ++idx) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    [[nodiscard]] static unsigned threadBudgetUnlocked() noexcept {
        unsigned hardware = std::thread::hardware_concurrency();
        if (hardware == 0) {
            hardware = DEFAULT_HW_THREADS;
        }
        if (const std::optional<unsigned>& override = parallelThreadOverride();
            override.has_value() && *override > 0) {
            hardware = *override;
        }
        return hardware;
    }

    void workerLoop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(queue_mu);
                queue_cv.wait(lock, [&] { return stopping || !queue.empty(); });
                if (stopping && queue.empty()) {
                    return;
                }
                job = std::move(queue.front());
                queue.pop();
            }
            job();
        }
    }

    std::mutex queue_mu;
    std::condition_variable queue_cv;
    std::queue<std::function<void()>> queue;
    std::vector<std::thread> workers;
    bool stopping{false};
};

} // namespace detail

/// Optional process-wide thread budget override (set from CLI / main).
inline void setParallelThreadOverride(std::optional<unsigned> threads) {
    detail::parallelThreadOverride() = threads;
}

[[nodiscard]] inline unsigned threadBudget() noexcept {
    unsigned hardware = std::thread::hardware_concurrency();
    if (hardware == 0) {
        hardware = DEFAULT_HW_THREADS;
    }
    if (const std::optional<unsigned>& override = detail::parallelThreadOverride();
        override.has_value() && *override > 0) {
        hardware = *override;
    }
    return hardware;
}

/// Dynamic work-stealing style loop over `[0, count)`. Sequential if `count` is tiny
/// or thread override / budget is 1.
template <class Callback>
void parallelFor(std::size_t count, Callback callback) {
    if (count == 0) {
        return;
    }
    const unsigned HARDWARE = threadBudget();
    if (count == 1 || HARDWARE == 1) {
        for (std::size_t idx = 0; idx < count; ++idx) {
            callback(idx);
        }
        return;
    }
    detail::ThreadPool::instance().parallelFor(count, std::function<void(std::size_t)>(callback));
}

} // namespace cyka
