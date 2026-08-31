#pragma once

#include <atomic>

namespace cyka::demo {

/// Set once from CLI/options before parsing demos (thread-safe reads during parse).
void setDebugEntLogging(bool enabled) noexcept;
[[nodiscard]] bool debugEntLogging() noexcept;

} // namespace cyka::demo
