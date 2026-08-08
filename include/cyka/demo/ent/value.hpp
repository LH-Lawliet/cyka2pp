#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cyka::demo::ent {

enum class ValKind : std::uint8_t { None, Bool, Int, Uint, Float, Vec3, Str };

/// Decoded field value. Deliberately a flat struct rather than a variant: only
/// entities we track keep state, so the extra bytes are irrelevant and reads
/// stay branch-cheap.
struct EntValue {
    ValKind kind{ValKind::None};
    bool b{false};
    float f{0};
    std::int64_t i{0};
    std::uint64_t u{0};
    std::array<float, 3> v3{};
    std::string s;

    [[nodiscard]] std::uint64_t as_u64() const noexcept {
        switch (kind) {
        case ValKind::Uint:
            return u;
        case ValKind::Int:
            return static_cast<std::uint64_t>(i);
        case ValKind::Bool:
            return b ? 1U : 0U;
        case ValKind::Float:
            return static_cast<std::uint64_t>(f);
        default:
            return 0;
        }
    }

    [[nodiscard]] std::int64_t as_i64() const noexcept {
        switch (kind) {
        case ValKind::Int:
            return i;
        case ValKind::Uint:
            return static_cast<std::int64_t>(u);
        case ValKind::Bool:
            return b ? 1 : 0;
        case ValKind::Float:
            return static_cast<std::int64_t>(f);
        default:
            return 0;
        }
    }

    [[nodiscard]] float as_f32() const noexcept {
        switch (kind) {
        case ValKind::Float:
            return f;
        case ValKind::Int:
            return static_cast<float>(i);
        case ValKind::Uint:
            return static_cast<float>(u);
        default:
            return 0;
        }
    }

    [[nodiscard]] bool as_bool() const noexcept { return kind == ValKind::Bool ? b : as_u64() != 0; }

    static EntValue of_bool(bool v) {
        EntValue x;
        x.kind = ValKind::Bool;
        x.b = v;
        return x;
    }
    static EntValue of_int(std::int64_t v) {
        EntValue x;
        x.kind = ValKind::Int;
        x.i = v;
        return x;
    }
    static EntValue of_uint(std::uint64_t v) {
        EntValue x;
        x.kind = ValKind::Uint;
        x.u = v;
        return x;
    }
    static EntValue of_float(float v) {
        EntValue x;
        x.kind = ValKind::Float;
        x.f = v;
        return x;
    }
    static EntValue of_vec3(std::array<float, 3> v) {
        EntValue x;
        x.kind = ValKind::Vec3;
        x.v3 = v;
        return x;
    }
    static EntValue of_str(std::string v) {
        EntValue x;
        x.kind = ValKind::Str;
        x.s = std::move(v);
        return x;
    }
};

} // namespace cyka::demo::ent
