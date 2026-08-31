#include "cyka/demo/mapped_file.hpp"

#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace cyka::demo {
namespace {

[[nodiscard]] int openReadOnly(const std::filesystem::path& path) {
    // Prefer fopen+fcntl over open/openat/syscall: those are vararg APIs.
    const std::unique_ptr<std::FILE, int (*)(std::FILE*)> STREAM{
        std::fopen(path.c_str(), "rbe"), &std::fclose};
    if (STREAM == nullptr) {
        return -1;
    }
    const int ORIG_FD = ::fileno(STREAM.get());
    if (ORIG_FD < 0) {
        return -1;
    }
    return ::fcntl(ORIG_FD, F_DUPFD_CLOEXEC, 0);
}

} // namespace

MappedFile::MappedFile(MappedFile&& other) noexcept
    : mapped_ptr(std::exchange(other.mapped_ptr, nullptr)),
      byte_count(std::exchange(other.byte_count, 0)),
      file_desc(std::exchange(other.file_desc, -1)) {}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        this->~MappedFile();
        mapped_ptr = std::exchange(other.mapped_ptr, nullptr);
        byte_count = std::exchange(other.byte_count, 0);
        file_desc = std::exchange(other.file_desc, -1);
    }
    return *this;
}

MappedFile::~MappedFile() {
    if (mapped_ptr != nullptr && mapped_ptr != MAP_FAILED) {
        ::munmap(mapped_ptr, byte_count);
    }
    if (file_desc >= 0) {
        ::close(file_desc);
    }
}

Result<MappedFile> mapFile(const std::filesystem::path& path) {
    const int FILE_DESC = openReadOnly(path);
    if (FILE_DESC < 0) {
        return std::unexpected(Error::NOT_FOUND);
    }

    struct stat stat_buf{};
    if (::fstat(FILE_DESC, &stat_buf) != 0 || stat_buf.st_size < 0) {
        ::close(FILE_DESC);
        return std::unexpected(Error::IO);
    }

    const auto BYTE_COUNT = static_cast<std::size_t>(stat_buf.st_size);
    if (BYTE_COUNT == 0) {
        MappedFile out;
        out.file_desc = FILE_DESC;
        return out;
    }

    void* ptr = ::mmap(nullptr, BYTE_COUNT, PROT_READ, MAP_PRIVATE, FILE_DESC, 0);
    if (ptr == MAP_FAILED) {
        ::close(FILE_DESC);
        return std::unexpected(Error::IO);
    }

    MappedFile out;
    out.mapped_ptr = static_cast<std::uint8_t*>(ptr);
    out.byte_count = BYTE_COUNT;
    out.file_desc = FILE_DESC;
    return out;
}

} // namespace cyka::demo
