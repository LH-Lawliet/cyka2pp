#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>
#include <vector>

namespace cyka {

inline constexpr unsigned DEFAULT_HW_THREADS = 4;

namespace detail {

inline std::optional<unsigned>& parallelThreadOverride() {
    static std::optional<unsigned> threads;
    return threads;
}

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
    const unsigned WORKERS = static_cast<unsigned>(std::min<std::size_t>(HARDWARE, count));
    std::atomic<std::size_t> next{0};
    std::vector<std::thread> threads;
    threads.reserve(WORKERS);
    for (unsigned worker = 0; worker < WORKERS; ++worker) {
        threads.emplace_back([&] {
            for (;;) {
                const std::size_t INDEX = next.fetch_add(1, std::memory_order_relaxed);
                if (INDEX >= count) {
                    break;
                }
                callback(INDEX);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
}

} // namespace cyka
