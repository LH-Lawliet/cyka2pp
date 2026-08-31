// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/field_decoder.go.
// See NOTICE.

#include "cyka/demo/ent/decoder.hpp"

#include "cyka/demo/ent/bit_floats.hpp"

#include <bit>

namespace cyka::demo::ent {
namespace {

inline constexpr float SIM_TIME_SCALE = 1.0F / 64.0F;
inline constexpr unsigned RUNE_TIME_BITS = 4U;
inline constexpr unsigned VEC3_COMPS = 3U;
inline constexpr unsigned ANGLE_PRECISE_BITS = 20U;
inline constexpr float ANGLE_HALF_TURN = 180.0F;
inline constexpr std::size_t VEC_X = 0;
inline constexpr std::size_t VEC_Y = 1;
inline constexpr std::size_t VEC_Z = 2;

float floatFromBits(std::uint32_t bits) noexcept {
    return std::bit_cast<float>(bits);
}

float decodeScalar(DecOp dec_op, const QuantizedFloat& quant, BitStream& reader) noexcept {
    switch (dec_op) {
    case DecOp::COORD:
        return readCoord(reader);
    case DecOp::SIM_TIME:
        return static_cast<float>(reader.readVarU32()) * SIM_TIME_SCALE;
    case DecOp::RUNE_TIME:
        return floatFromBits(reader.readBits(RUNE_TIME_BITS));
    case DecOp::QUANTIZED:
        return quant.decode(reader);
    case DecOp::NO_SCALE:
    default: {
        const std::uint32_t BITS = reader.readLeU32();
        return BITS == 0 ? 0.0F : floatFromBits(BITS);
    }
    }
}

} // namespace

EntValue decodeValue(const DecoderSpec& spec, BitStream& reader) {
    switch (spec.op) {
    case DecOp::BOOL:
        return EntValue::ofBool(reader.readBool());
    case DecOp::SIGNED:
        return EntValue::ofInt(reader.readVarI32());
    case DecOp::UNSIGNED:
        return EntValue::ofUint(reader.readVarU32());
    case DecOp::UNSIGNED64:
        return EntValue::ofUint(reader.readVarU64());
    case DecOp::FIXED64:
        return EntValue::ofUint(reader.readLeU64());
    case DecOp::STRING:
        return EntValue::ofStr(reader.readString());
    case DecOp::BINARY_BLOCK: {
        const std::uint32_t NUM = reader.readVarU32();
        reader.skipBytes(NUM);
        return EntValue::ofUint(NUM);
    }
    case DecOp::AMMO: {
        const std::uint32_t VAL = reader.readVarU32();
        return EntValue::ofInt(static_cast<std::int64_t>(VAL) - 1);
    }
    case DecOp::COMPONENT:
        return EntValue::ofUint(reader.readBits(1));
    case DecOp::POLY_BASE: {
        if (!reader.readBool()) {
            return EntValue::ofBool(false);
        }
        EntValue val = EntValue::ofUint(reader.readUbitVar());
        val.b = true;
        return val;
    }
    case DecOp::VEC3_NORMAL:
        return EntValue::ofVec3(read3bitNormal(reader));
    case DecOp::VECTOR: {
        std::array<float, VEC3_COMPS> vec{};
        const unsigned COMPS = spec.comps == 0 ? VEC3_COMPS : spec.comps;
        for (unsigned idx = 0; idx < COMPS; ++idx) {
            const float COMP = decodeScalar(spec.sub, spec.qf, reader);
            if (idx < VEC3_COMPS) {
                vec[idx] = COMP;
            }
        }
        return EntValue::ofVec3(vec);
    }
    case DecOp::Q_ANGLE_BITS: {
        std::array<float, VEC3_COMPS> vec{};
        for (auto& comp : vec) {
            comp = readAngle(reader, spec.bits);
        }
        return EntValue::ofVec3(vec);
    }
    case DecOp::Q_ANGLE_PRECISE: {
        std::array<float, VEC3_COMPS> vec{};
        const bool HAS_X = reader.readBool();
        const bool HAS_Y = reader.readBool();
        const bool HAS_Z = reader.readBool();
        if (HAS_X) {
            vec[VEC_X] = readAngle(reader, ANGLE_PRECISE_BITS) - ANGLE_HALF_TURN;
        }
        if (HAS_Y) {
            vec[VEC_Y] = readAngle(reader, ANGLE_PRECISE_BITS) - ANGLE_HALF_TURN;
        }
        if (HAS_Z) {
            vec[VEC_Z] = readAngle(reader, ANGLE_PRECISE_BITS) - ANGLE_HALF_TURN;
        }
        return EntValue::ofVec3(vec);
    }
    case DecOp::Q_ANGLE_COORD: {
        std::array<float, VEC3_COMPS> vec{};
        const bool HAS_X = reader.readBool();
        const bool HAS_Y = reader.readBool();
        const bool HAS_Z = reader.readBool();
        if (HAS_X) {
            vec[VEC_X] = readCoord(reader);
        }
        if (HAS_Y) {
            vec[VEC_Y] = readCoord(reader);
        }
        if (HAS_Z) {
            vec[VEC_Z] = readCoord(reader);
        }
        return EntValue::ofVec3(vec);
    }
    case DecOp::NO_SCALE:
    case DecOp::COORD:
    case DecOp::SIM_TIME:
    case DecOp::RUNE_TIME:
    case DecOp::QUANTIZED:
        return EntValue::ofFloat(decodeScalar(spec.op, spec.qf, reader));
    case DecOp::DEFAULT:
    default:
        return EntValue::ofUint(reader.readVarU32());
    }
}

} // namespace cyka::demo::ent
