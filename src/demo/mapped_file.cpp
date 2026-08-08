#include "cyka/demo/mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace cyka::demo {

MappedFile::MappedFile(MappedFile&& o) noexcept
    : data_(std::exchange(o.data_, nullptr)), size_(std::exchange(o.size_, 0)),
      fd_(std::exchange(o.fd_, -1)) {}

MappedFile& MappedFile::operator=(MappedFile&& o) noexcept {
    if (this != &o) {
        this->~MappedFile();
        data_ = std::exchange(o.data_, nullptr);
        size_ = std::exchange(o.size_, 0);
        fd_ = std::exchange(o.fd_, -1);
    }
    return *this;
}

MappedFile::~MappedFile() {
    if (data_ != nullptr && data_ != MAP_FAILED) {
        ::munmap(data_, size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

Result<MappedFile> map_file(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return std::unexpected(Error::NotFound);
    }
    struct stat st {};
    if (::fstat(fd, &st) != 0 || st.st_size < 0) {
        ::close(fd);
        return std::unexpected(Error::Io);
    }
    const auto size = static_cast<std::size_t>(st.st_size);
    if (size == 0) {
        MappedFile out;
        out.fd_ = fd;
        return out;
    }
    void* p = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
        ::close(fd);
        return std::unexpected(Error::Io);
    }
    MappedFile out;
    out.data_ = static_cast<std::uint8_t*>(p);
    out.size_ = size;
    out.fd_ = fd;
    return out;
}

} // namespace cyka::demo
