#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_decoder.go. See NOTICE.

#include "cyka/demo/ent/bit_floats.hpp"
#include "cyka/demo/ent/bit_stream.hpp"
#include "cyka/demo/ent/quantized_float.hpp"
#include "cyka/demo/ent/value.hpp"

#include <cstdint>

namespace cyka::demo::ent {

enum class DecOp : std::uint8_t {
    Default, // varint u32
    Bool,
    Signed,
    Unsigned,
    Unsigned64,
    Fixed64,
    String,
    BinaryBlock,
    NoScale,
    Coord,
    SimTime,
    RuneTime,
    Quantized,
    Ammo,
    Component,
    Vector, // `comps` scalars decoded with `sub`
    Vec3Normal,
    QAngleCoord,
    QAngleBits,
    QAnglePrecise,
    PolyBase, // bool, then ubitvar type index only if the pointer is active
};

/// A resolved decoder. `sub`/`qf` carry the scalar flavour for vector fields.
struct DecoderSpec {
    DecOp op{DecOp::Default};
    DecOp sub{DecOp::NoScale};
    std::uint8_t comps{0};
    std::uint32_t bits{0};
    QuantizedFloat qf{};
};

[[nodiscard]] EntValue decode_value(const DecoderSpec& spec, BitStream& r);

} // namespace cyka::demo::ent
