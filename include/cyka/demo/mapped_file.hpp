#pragma once

#include "cyka/error.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace cyka::demo {

/// Read-only memory-mapped file. Owns the mapping; views via `bytes()`.
class MappedFile {
public:
    MappedFile() = default;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&&) noexcept;
    MappedFile& operator=(MappedFile&&) noexcept;
    ~MappedFile();

    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept {
        return {data_, size_};
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

private:
    friend Result<MappedFile> map_file(const std::filesystem::path& path);

    std::uint8_t* data_{nullptr};
    std::size_t size_{0};
    int fd_{-1};
};

/// mmap(PROT_READ) a demo file. Returns Error::Io / NotFound on failure.
[[nodiscard]] Result<MappedFile> map_file(const std::filesystem::path& path);

} // namespace cyka::demo
