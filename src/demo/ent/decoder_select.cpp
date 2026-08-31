// Type/encoder → decoder mapping, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_decoder.go. See NOTICE.

#include "cyka/demo/ent/decoder_types.hpp"
#include "cyka/demo/ent/field.hpp"

#include <string_view>

namespace cyka::demo::ent {
namespace {

DecoderSpec simple(DecOp op) {
    DecoderSpec s;
    s.op = op;
    return s;
}

DecoderSpec quantized_factory(const EntField& f) {
    if (!f.bit_count || *f.bit_count <= 0 || *f.bit_count >= 32) {
        return simple(DecOp::NoScale);
    }
    DecoderSpec s;
    s.op = DecOp::Quantized;
    s.qf = make_quantized_float(*f.bit_count, f.encode_flags, f.low_value, f.high_value);
    return s;
}

DecoderSpec float_factory(const EntField& f) {
    if (f.encoder == "coord") {
        return simple(DecOp::Coord);
    }
    if (f.encoder == "simtime") {
        return simple(DecOp::SimTime);
    }
    if (f.encoder == "runetime") {
        return simple(DecOp::RuneTime);
    }
    if (!f.bit_count || *f.bit_count <= 0 || *f.bit_count >= 32) {
        return simple(DecOp::NoScale);
    }
    return quantized_factory(f);
}

DecoderSpec vector_factory(const EntField& f, unsigned n) {
    if (n == 3 && f.encoder == "normal") {
        return simple(DecOp::Vec3Normal);
    }
    const DecoderSpec scalar = float_factory(f);
    DecoderSpec s;
    s.op = DecOp::Vector;
    s.sub = scalar.op;
    s.qf = scalar.qf;
    s.comps = static_cast<std::uint8_t>(n);
    return s;
}

DecoderSpec qangle_factory(const EntField& f) {
    if (f.encoder == "qangle_precise") {
        return simple(DecOp::QAnglePrecise);
    }
    if (f.bit_count && *f.bit_count != 0) {
        // 32-bit (or wider) components are noscale IEEE float32s, not scaled
        // bit-angles. read_angle(32) would map the raw bits into [0,360) and
        // garble m_aimPunchAngle / similar. Mirror float_factory's noscale guard.
        if (*f.bit_count >= 32) {
            DecoderSpec s;
            s.op = DecOp::Vector;
            s.sub = DecOp::NoScale;
            s.comps = 3;
            return s;
        }
        DecoderSpec s;
        s.op = DecOp::QAngleBits;
        s.bits = static_cast<std::uint32_t>(*f.bit_count);
        return s;
    }
    return simple(DecOp::QAngleCoord);
}

DecoderSpec unsigned64_factory(const EntField& f) {
    return simple(f.encoder == "fixed64" ? DecOp::Fixed64 : DecOp::Unsigned64);
}

/// Base types whose decoder depends on the field's encoder / bit count.
bool factory_for(std::string_view base, const EntField& f, DecoderSpec& out) {
    if (base == "float32") {
        out = float_factory(f);
    } else if (base == "CNetworkedQuantizedFloat") {
        out = quantized_factory(f);
    } else if (base == "uint64" || base == "CStrongHandle") {
        out = unsigned64_factory(f);
    } else if (base == "Vector" || base == "VectorWS") {
        out = vector_factory(f, 3);
    } else if (base == "Vector2D") {
        out = vector_factory(f, 2);
    } else if (base == "Vector4D" || base == "Quaternion") {
        out = vector_factory(f, 4);
    } else if (base == "CTransform") {
        out = vector_factory(f, 6);
    } else if (base == "QAngle") {
        out = qangle_factory(f);
    } else {
        return false;
    }
    return true;
}

} // namespace

DecoderSpec find_decoder(const EntField& f) {
    const std::string_view base = f.type ? std::string_view{f.type->base} : std::string_view{};
    DecoderSpec out;
    if (factory_for(base, f, out)) {
        return out;
    }
    if (f.var_name == "m_iClip1") {
        return simple(DecOp::Ammo);
    }
    if (const auto it = type_decoders().find(base); it != type_decoders().end()) {
        return simple(it->second);
    }
    return simple(DecOp::Default);
}

DecoderSpec find_decoder_by_base(const EntField& f) {
    if (!f.type || !f.type->generic) {
        return simple(DecOp::Default);
    }
    const std::string_view base = f.type->generic->base;
    DecoderSpec out;
    if (factory_for(base, f, out)) {
        return out;
    }
    if (const auto it = type_decoders().find(base); it != type_decoders().end()) {
        return simple(it->second);
    }
    return simple(DecOp::Default);
}

} // namespace cyka::demo::ent
