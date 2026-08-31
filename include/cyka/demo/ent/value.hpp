#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cyka::demo::ent {

enum class ValKind : std::uint8_t { NONE, BOOL, INT, UINT, FLOAT, VEC3, STR };

inline constexpr int ENT_VALUE_VEC3_COMPONENTS = 3;

/// Decoded field value. Deliberately a flat struct rather than a variant: only
/// entities we track keep state, so the extra bytes are irrelevant and reads
/// stay branch-cheap.
struct EntValue {
    ValKind kind{ValKind::NONE};
    bool b{false};
    float f{0};
    std::int64_t i{0};
    std::uint64_t u{0};
    std::array<float, ENT_VALUE_VEC3_COMPONENTS> v3{};
    std::string s;

    [[nodiscard]] std::uint64_t asU64() const noexcept {
        switch (kind) {
        case ValKind::UINT:
            return u;
        case ValKind::INT:
            return static_cast<std::uint64_t>(i);
        case ValKind::BOOL:
            return b ? 1U : 0U;
        case ValKind::FLOAT:
            return static_cast<std::uint64_t>(f);
        default:
            return 0;
        }
    }

    [[nodiscard]] std::int64_t asI64() const noexcept {
        switch (kind) {
        case ValKind::INT:
            return i;
        case ValKind::UINT:
            return static_cast<std::int64_t>(u);
        case ValKind::BOOL:
            return b ? 1 : 0;
        case ValKind::FLOAT:
            return static_cast<std::int64_t>(f);
        default:
            return 0;
        }
    }

    [[nodiscard]] float asF32() const noexcept {
        switch (kind) {
        case ValKind::FLOAT:
            return f;
        case ValKind::INT:
            return static_cast<float>(i);
        case ValKind::UINT:
            return static_cast<float>(u);
        default:
            return 0;
        }
    }

    [[nodiscard]] bool asBool() const noexcept { return kind == ValKind::BOOL ? b : asU64() != 0; }

    static EntValue ofBool(bool value) {
        EntValue out;
        out.kind = ValKind::BOOL;
        out.b = value;
        return out;
    }
    static EntValue ofInt(std::int64_t value) {
        EntValue out;
        out.kind = ValKind::INT;
        out.i = value;
        return out;
    }
    static EntValue ofUint(std::uint64_t value) {
        EntValue out;
        out.kind = ValKind::UINT;
        out.u = value;
        return out;
    }
    static EntValue ofFloat(float value) {
        EntValue out;
        out.kind = ValKind::FLOAT;
        out.f = value;
        return out;
    }
    static EntValue ofVec3(std::array<float, ENT_VALUE_VEC3_COMPONENTS> value) {
        EntValue out;
        out.kind = ValKind::VEC3;
        out.v3 = value;
        return out;
    }
    static EntValue ofStr(std::string value) {
        EntValue out;
        out.kind = ValKind::STR;
        out.s = std::move(value);
        return out;
    }
};

} // namespace cyka::demo::ent
