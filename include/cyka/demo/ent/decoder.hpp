#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_decoder.go. See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"
#include "cyka/demo/ent/quantized_float.hpp"
#include "cyka/demo/ent/value.hpp"

#include <cstdint>

namespace cyka::demo::ent {

enum class DecOp : std::uint8_t {
    DEFAULT, // varint u32
    BOOL,
    SIGNED,
    UNSIGNED,
    UNSIGNED64,
    FIXED64,
    STRING,
    BINARY_BLOCK,
    NO_SCALE,
    COORD,
    SIM_TIME,
    RUNE_TIME,
    QUANTIZED,
    AMMO,
    COMPONENT,
    VECTOR, // `comps` scalars decoded with `sub`
    VEC3_NORMAL,
    Q_ANGLE_COORD,
    Q_ANGLE_BITS,
    Q_ANGLE_PRECISE,
    POLY_BASE, // bool, then ubitvar type index only if the pointer is active
};

/// A resolved decoder. `sub`/`qf` carry the scalar flavour for vector fields.
struct DecoderSpec {
    DecOp op{DecOp::DEFAULT};
    DecOp sub{DecOp::NO_SCALE};
    std::uint8_t comps{0};
    std::uint32_t bits{0};
    QuantizedFloat qf{};
};

[[nodiscard]] EntValue decodeValue(const DecoderSpec& spec, BitStream& reader);

} // namespace cyka::demo::ent
