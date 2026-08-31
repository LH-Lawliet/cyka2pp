#include "cyka/demo/debug.hpp"

namespace cyka::demo {
namespace {

[[nodiscard]] std::atomic<bool>& debugEntLoggingFlag() noexcept {
    static std::atomic<bool> enabled{false};
    return enabled;
}

} // namespace

void setDebugEntLogging(bool enabled) noexcept {
    debugEntLoggingFlag().store(enabled, std::memory_order_relaxed);
}

bool debugEntLogging() noexcept {
    return debugEntLoggingFlag().load(std::memory_order_relaxed);
}

} // namespace cyka::demo
