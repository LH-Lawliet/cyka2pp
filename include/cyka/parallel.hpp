#pragma once

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

namespace cyka {

/// Dynamic work-stealing style loop over `[0, n)`. Sequential if `n` is tiny
/// or `CYKA_THREADS=1`.
template <class Fn>
void parallel_for(std::size_t n, Fn&& fn) {
    if (n == 0) {
        return;
    }
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 4;
    }
    if (const char* env = std::getenv("CYKA_THREADS")) {
        const int thread_count = std::atoi(env);
        if (thread_count > 0) {
            hw = static_cast<unsigned>(thread_count);
        }
    }
    if (n == 1 || hw == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            fn(i);
        }
        return;
    }
    const unsigned workers = static_cast<unsigned>(std::min<std::size_t>(hw, n));
    std::atomic<std::size_t> next{0};
    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (unsigned t = 0; t < workers; ++t) {
        threads.emplace_back([&] {
            for (;;) {
                const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= n) {
                    break;
                }
                fn(i);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
}

} // namespace cyka
