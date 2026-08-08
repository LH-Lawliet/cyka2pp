#pragma once

#include <expected>
#include <string_view>

namespace cyka {

/// Analyzer / IO failure codes. Message helpers live in error.cpp if needed later.
enum class Error {
    Ok = 0,
    Io,
    InvalidArgument,
    Parse,
    Unsupported,
    Mesh,
    NotFound,
};

[[nodiscard]] constexpr std::string_view to_string(Error e) noexcept {
    switch (e) {
    case Error::Ok:
        return "ok";
    case Error::Io:
        return "io";
    case Error::InvalidArgument:
        return "invalid_argument";
    case Error::Parse:
        return "parse";
    case Error::Unsupported:
        return "unsupported";
    case Error::Mesh:
        return "mesh";
    case Error::NotFound:
        return "not_found";
    }
    return "unknown";
}

/// Result<T> is the project-wide fallible return type (no exceptions on hot paths).
template <typename T>
using Result = std::expected<T, Error>;

} // namespace cyka
