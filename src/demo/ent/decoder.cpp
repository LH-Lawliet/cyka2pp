// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/field_decoder.go.
// See NOTICE.

#include "cyka/demo/ent/decoder.hpp"

#include <bit>

namespace cyka::demo::ent {
namespace {

float float_from_bits(std::uint32_t bits) noexcept {
    return std::bit_cast<float>(bits);
}

float decode_scalar(DecOp op, const QuantizedFloat& qf, BitStream& r) noexcept {
    switch (op) {
    case DecOp::Coord:
        return read_coord(r);
    case DecOp::SimTime:
        return static_cast<float>(r.read_var_u32()) * (1.0F / 64.0F);
    case DecOp::RuneTime:
        return float_from_bits(r.read_bits(4));
    case DecOp::Quantized:
        return qf.decode(r);
    case DecOp::NoScale:
    default: {
        const std::uint32_t bits = r.read_le_u32();
        return bits == 0 ? 0.0F : float_from_bits(bits);
    }
    }
}

} // namespace

EntValue decode_value(const DecoderSpec& spec, BitStream& r) {
    switch (spec.op) {
    case DecOp::Bool:
        return EntValue::of_bool(r.read_bool());
    case DecOp::Signed:
        return EntValue::of_int(r.read_var_i32());
    case DecOp::Unsigned:
        return EntValue::of_uint(r.read_var_u32());
    case DecOp::Unsigned64:
        return EntValue::of_uint(r.read_var_u64());
    case DecOp::Fixed64:
        return EntValue::of_uint(r.read_le_u64());
    case DecOp::String:
        return EntValue::of_str(r.read_string());
    case DecOp::BinaryBlock: {
        const std::uint32_t n = r.read_var_u32();
        r.skip_bytes(n);
        return EntValue::of_uint(n);
    }
    case DecOp::Ammo: {
        const std::uint32_t v = r.read_var_u32();
        return EntValue::of_int(static_cast<std::int64_t>(v) - 1);
    }
    case DecOp::Component:
        return EntValue::of_uint(r.read_bits(1));
    case DecOp::PolyBase: {
        // Inactive pointer is a single false bit. The ubitvar type index is
        // only present when the pointer is active (demoinfocs / Clarity).
        if (!r.read_bool()) {
            return EntValue::of_bool(false);
        }
        EntValue v = EntValue::of_uint(r.read_ubit_var());
        v.b = true;
        return v;
    }
    case DecOp::Vec3Normal:
        return EntValue::of_vec3(read_3bit_normal(r));
    case DecOp::Vector: {
        std::array<float, 3> v{};
        const unsigned comps = spec.comps == 0 ? 3U : spec.comps;
        for (unsigned i = 0; i < comps; ++i) {
            const float x = decode_scalar(spec.sub, spec.qf, r);
            if (i < 3) {
                v[i] = x;
            }
        }
        return EntValue::of_vec3(v);
    }
    case DecOp::QAngleBits: {
        std::array<float, 3> v{};
        for (auto& c : v) {
            c = read_angle(r, spec.bits);
        }
        return EntValue::of_vec3(v);
    }
    case DecOp::QAnglePrecise: {
        std::array<float, 3> v{};
        const bool has_x = r.read_bool();
        const bool has_y = r.read_bool();
        const bool has_z = r.read_bool();
        if (has_x) {
            v[0] = read_angle(r, 20) - 180.0F;
        }
        if (has_y) {
            v[1] = read_angle(r, 20) - 180.0F;
        }
        if (has_z) {
            v[2] = read_angle(r, 20) - 180.0F;
        }
        return EntValue::of_vec3(v);
    }
    case DecOp::QAngleCoord: {
        std::array<float, 3> v{};
        const bool has_x = r.read_bool();
        const bool has_y = r.read_bool();
        const bool has_z = r.read_bool();
        if (has_x) {
            v[0] = read_coord(r);
        }
        if (has_y) {
            v[1] = read_coord(r);
        }
        if (has_z) {
            v[2] = read_coord(r);
        }
        return EntValue::of_vec3(v);
    }
    case DecOp::NoScale:
    case DecOp::Coord:
    case DecOp::SimTime:
    case DecOp::RuneTime:
    case DecOp::Quantized:
        return EntValue::of_float(decode_scalar(spec.op, spec.qf, r));
    case DecOp::Default:
    default:
        return EntValue::of_uint(r.read_var_u32());
    }
}

} // namespace cyka::demo::ent
