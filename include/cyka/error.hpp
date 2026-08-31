#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace cyka {

/// Analyzer / IO failure codes. Message helpers live in error.cpp if needed later.
enum class Error : std::uint8_t {
    OK = 0,
    IO,
    INVALID_ARGUMENT,
    PARSE,
    UNSUPPORTED,
    MESH,
    NOT_FOUND,
};

[[nodiscard]] constexpr std::string_view toString(Error err) noexcept {
    switch (err) {
    case Error::OK:
        return "ok";
    case Error::IO:
        return "io";
    case Error::INVALID_ARGUMENT:
        return "invalid_argument";
    case Error::PARSE:
        return "parse";
    case Error::UNSUPPORTED:
        return "unsupported";
    case Error::MESH:
        return "mesh";
    case Error::NOT_FOUND:
        return "not_found";
    }
    return "unknown";
}

/// Result<T> is the project-wide fallible return type (no exceptions on hot paths).
template <typename T>
using Result = std::expected<T, Error>;

} // namespace cyka
