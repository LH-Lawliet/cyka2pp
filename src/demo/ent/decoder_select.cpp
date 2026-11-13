// Type/encoder → decoder mapping, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_decoder.go. See NOTICE.

#include "cyka/demo/ent/decoder_types.hpp"
#include "cyka/demo/ent/field.hpp"

#include <string_view>

namespace cyka::demo::ent {
namespace {

inline constexpr int FLOAT_BITS = 32;
inline constexpr unsigned VEC3_COMPS = 3U;
inline constexpr unsigned VEC2_COMPS = 2U;
inline constexpr unsigned VEC4_COMPS = 4U;
inline constexpr unsigned TRANSFORM_COMPS = 6U;

DecoderSpec simple(DecOp dec_op) {
    DecoderSpec spec;
    spec.op = dec_op;
    return spec;
}

DecoderSpec quantizedFactory(const EntField& field) {
    if (!field.bit_count || *field.bit_count <= 0 || *field.bit_count >= FLOAT_BITS) {
        return simple(DecOp::NO_SCALE);
    }
    DecoderSpec spec;
    spec.op = DecOp::QUANTIZED;
    spec.qf =
        makeQuantizedFloat(*field.bit_count, field.encode_flags, field.low_value, field.high_value);
    return spec;
}

DecoderSpec floatFactory(const EntField& field) {
    if (field.encoder == "coord") {
        return simple(DecOp::COORD);
    }
    if (field.encoder == "simtime") {
        return simple(DecOp::SIM_TIME);
    }
    if (field.encoder == "runetime") {
        return simple(DecOp::RUNE_TIME);
    }
    if (!field.bit_count || *field.bit_count <= 0 || *field.bit_count >= FLOAT_BITS) {
        return simple(DecOp::NO_SCALE);
    }
    return quantizedFactory(field);
}

DecoderSpec vectorFactory(const EntField& field, unsigned num_comps) {
    if (num_comps == VEC3_COMPS && field.encoder == "normal") {
        return simple(DecOp::VEC3_NORMAL);
    }
    const DecoderSpec SCALAR = floatFactory(field);
    DecoderSpec spec;
    spec.op = DecOp::VECTOR;
    spec.sub = SCALAR.op;
    spec.qf = SCALAR.qf;
    spec.comps = static_cast<std::uint8_t>(num_comps);
    return spec;
}

DecoderSpec qangleFactory(const EntField& field) {
    if (field.encoder == "qangle_precise") {
        return simple(DecOp::Q_ANGLE_PRECISE);
    }
    if (field.bit_count && *field.bit_count != 0) {
        if (*field.bit_count >= FLOAT_BITS) {
            DecoderSpec spec;
            spec.op = DecOp::VECTOR;
            spec.sub = DecOp::NO_SCALE;
            spec.comps = static_cast<std::uint8_t>(VEC3_COMPS);
            return spec;
        }
        DecoderSpec spec;
        spec.op = DecOp::Q_ANGLE_BITS;
        spec.bits = static_cast<std::uint32_t>(*field.bit_count);
        return spec;
    }
    return simple(DecOp::Q_ANGLE_COORD);
}

DecoderSpec unsigned64Factory(const EntField& field) {
    return simple(field.encoder == "fixed64" ? DecOp::FIXED64 : DecOp::UNSIGNED64);
}

/// Base types whose decoder depends on the field's encoder / bit count.
bool factoryFor(std::string_view base, const EntField& field, DecoderSpec& out) {
    if (base == "float32") {
        out = floatFactory(field);
    } else if (base == "CNetworkedQuantizedFloat") {
        out = quantizedFactory(field);
    } else if (base == "uint64" || base == "CStrongHandle") {
        out = unsigned64Factory(field);
    } else if (base == "Vector" || base == "VectorWS") {
        out = vectorFactory(field, VEC3_COMPS);
    } else if (base == "Vector2D") {
        out = vectorFactory(field, VEC2_COMPS);
    } else if (base == "Vector4D" || base == "Quaternion") {
        out = vectorFactory(field, VEC4_COMPS);
    } else if (base == "CTransform") {
        out = vectorFactory(field, TRANSFORM_COMPS);
    } else if (base == "QAngle") {
        out = qangleFactory(field);
    } else {
        return false;
    }
    return true;
}

/// fixedN encoders store N raw bits instead of a varint. CS2 sends most 8-bit
/// fields this way since the 2026-11-11 update (e.g. m_recentKillQueue,
/// m_iTeamNum, m_iCompetitiveRankType); read as a varint, any value >= 128
/// consumes an extra byte and misaligns the rest of the packet.
bool fixedWidthFactory(const EntField& field, std::string_view base, DecoderSpec& out) {
    std::uint32_t bits = 0;
    if (field.encoder == "fixed8") {
        bits = 8;
    } else if (field.encoder == "fixed16") {
        bits = 16;
    } else if (field.encoder == "fixed32") {
        bits = 32;
    } else {
        return false;
    }

    // Types with their own decoder (float / vector / uint64 / ...) keep it; the
    // fixedN encoder only replaces the integer varint decoders.
    DecoderSpec factory;
    if (factoryFor(base, field, factory)) {
        return false;
    }
    const auto ITER = typeDecoders().find(base);
    if (ITER != typeDecoders().end()) {
        if (ITER->second == DecOp::SIGNED) {
            out = simple(DecOp::FIXED_SIGNED);
        } else if (ITER->second == DecOp::UNSIGNED) {
            out = simple(DecOp::FIXED_UNSIGNED);
        } else {
            return false;
        }
    } else {
        // Unmapped enum: the default varint decoder is unsigned.
        out = simple(DecOp::FIXED_UNSIGNED);
    }
    out.bits = bits;
    return true;
}

} // namespace

DecoderSpec findDecoder(const EntField& field) {
    const std::string_view BASE =
        field.type ? std::string_view{field.type->base} : std::string_view{};
    DecoderSpec out;
    if (fixedWidthFactory(field, BASE, out)) {
        return out;
    }
    if (factoryFor(BASE, field, out)) {
        return out;
    }
    if (field.var_name == "m_iClip1") {
        return simple(DecOp::AMMO);
    }
    if (const auto ITER = typeDecoders().find(BASE); ITER != typeDecoders().end()) {
        return simple(ITER->second);
    }
    return simple(DecOp::DEFAULT);
}

DecoderSpec findDecoderByBase(const EntField& field) {
    if (!field.type || !field.type->generic) {
        return simple(DecOp::DEFAULT);
    }
    const std::string_view BASE = field.type->generic->base;
    DecoderSpec out;
    if (fixedWidthFactory(field, BASE, out)) {
        return out;
    }
    if (factoryFor(BASE, field, out)) {
        return out;
    }
    if (const auto ITER = typeDecoders().find(BASE); ITER != typeDecoders().end()) {
        return simple(ITER->second);
    }
    return simple(DecOp::DEFAULT);
}

} // namespace cyka::demo::ent
