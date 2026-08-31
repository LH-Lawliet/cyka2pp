#pragma once

#include "cyka/demo/command.hpp"
#include "cyka/error.hpp"
#include "cyka/types.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace cyka::demo {

/// One decompressed demo command payload (owned if snappy was needed).
struct DemoFrame {
    DemoCommand cmd{DemoCommand::ERROR};
    Tick tick{0};
    std::span<const std::uint8_t> payload; // views `owned` or the mapped file
    std::vector<std::uint8_t> owned;       // non-empty when decompressed
};

/// Iterate PBDEMS2 commands: magic + 8-byte skip, then cmd/tick/size/body.
class DemoStream {
  public:
    explicit DemoStream(std::span<const std::uint8_t> file) noexcept;

    [[nodiscard]] bool ok() const noexcept { return stream_ok; }
    [[nodiscard]] Error error() const noexcept { return stream_err; }

    /// Returns false on Stop / EOF / error. `out.payload` valid until next next().
    [[nodiscard]] bool next(DemoFrame& out);

  private:
    std::span<const std::uint8_t> file_bytes;
    std::size_t byte_pos{0};
    bool stream_ok{true};
    Error stream_err{Error::OK};
};

/// Convenience: invoke `fn` for every frame until Stop.
[[nodiscard]] Result<void> forEachFrame(std::span<const std::uint8_t> file,
                                        const std::function<void(const DemoFrame&)>& callback);

} // namespace cyka::demo
