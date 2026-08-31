#pragma once

#include "cyka/error.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace cyka::demo {

/// Read-only memory-mapped file. Owns the mapping; views via `bytes()`.
class MappedFile {
  public:
    MappedFile() = default;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    ~MappedFile();

    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept {
        return {mapped_ptr, byte_count};
    }
    [[nodiscard]] std::size_t size() const noexcept { return byte_count; }
    [[nodiscard]] bool empty() const noexcept { return byte_count == 0; }

  private:
    friend Result<MappedFile> mapFile(const std::filesystem::path& path);

    std::uint8_t* mapped_ptr{nullptr};
    std::size_t byte_count{0};
    int file_desc{-1};
};

/// mmap(PROT_READ) a demo file. Returns Error::Io / NotFound on failure.
[[nodiscard]] Result<MappedFile> mapFile(const std::filesystem::path& path);

} // namespace cyka::demo
