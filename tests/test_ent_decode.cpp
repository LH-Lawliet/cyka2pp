#include "test_harness.hpp"

#include "cyka/demo/ent/decoder.hpp"
#include "cyka/demo/ent/field.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <vector>

namespace {

using cyka::demo::ent::BitStream;
using cyka::demo::ent::decode_value;
using cyka::demo::ent::DecoderSpec;
using cyka::demo::ent::DecOp;
using cyka::demo::ent::EntField;
using cyka::demo::ent::EntSerializer;
using cyka::demo::ent::FieldModel;
using cyka::demo::ent::FieldPath;
using cyka::demo::ent::find_decoder;
using cyka::demo::ent::parse_field_type;

void append_f32_le(std::vector<std::uint8_t>& out, float v) {
    const auto bits = std::bit_cast<std::uint32_t>(v);
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
    }
}

void test_qangle_noscale_32() {
    EntField f;
    f.var_type = "QAngle";
    f.type = parse_field_type("QAngle");
    f.bit_count = 32;
    const DecoderSpec spec = find_decoder(f);
    CYKA_CHECK(spec.op == DecOp::Vector);
    CYKA_CHECK(spec.sub == DecOp::NoScale);
    CYKA_CHECK(spec.comps == 3);

    std::vector<std::uint8_t> raw;
    append_f32_le(raw, 1.5F);
    append_f32_le(raw, -4.25F);
    append_f32_le(raw, 350.0F);
    BitStream r(raw);
    const auto val = decode_value(spec, r);
    CYKA_CHECK(!r.failed());
    CYKA_CHECK(val.v3[0] == 1.5F);
    CYKA_CHECK(val.v3[1] == -4.25F);
    CYKA_CHECK(val.v3[2] == 350.0F);
}

void test_qangle_scaled_20() {
    EntField f;
    f.var_type = "QAngle";
    f.type = parse_field_type("QAngle");
    f.bit_count = 20;
    const DecoderSpec spec = find_decoder(f);
    CYKA_CHECK(spec.op == DecOp::QAngleBits);
    CYKA_CHECK(spec.bits == 20);

    // 20-bit 0x80000 maps to 180 degrees; two more components follow.
    const std::array<std::uint8_t, 8> raw{0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00};
    BitStream r(raw);
    const auto val = decode_value(spec, r);
    CYKA_CHECK(val.v3[0] >= 0.0F && val.v3[0] < 360.0F);
    CYKA_CHECK(val.v3[1] >= 0.0F && val.v3[1] < 360.0F);
    CYKA_CHECK(val.v3[2] >= 0.0F && val.v3[2] < 360.0F);
}

void test_poly_wire_inactive_skips_ubitvar() {
    DecoderSpec spec;
    spec.op = DecOp::PolyBase;
    // bit0 = 0 (inactive), bit1 = 1. Old decoder would swallow bit1 as ubitvar.
    const std::array<std::uint8_t, 1> raw{0x02};
    BitStream r(raw);
    const auto v = decode_value(spec, r);
    CYKA_CHECK(!v.b);
    CYKA_CHECK(r.read_bool());
    CYKA_CHECK(!r.failed());
}

void test_poly_wire_active_index() {
    DecoderSpec spec;
    spec.op = DecOp::PolyBase;
    {
        const std::array<std::uint8_t, 1> raw{0x01}; // active, index 0
        BitStream r(raw);
        const auto v = decode_value(spec, r);
        CYKA_CHECK(v.b);
        CYKA_CHECK(v.u == 0);
    }
    {
        const std::array<std::uint8_t, 1> raw{0x03}; // active, index 1
        BitStream r(raw);
        const auto v = decode_value(spec, r);
        CYKA_CHECK(v.b);
        CYKA_CHECK(v.u == 1);
    }
}

void test_poly_per_entity_select() {
    EntField foo;
    foo.var_name = "m_nFoo";
    foo.var_type = "uint32";
    foo.type = parse_field_type("uint32");
    foo.set_model(FieldModel::Simple);

    EntField bar;
    bar.var_name = "m_nBar";
    bar.var_type = "int32";
    bar.type = parse_field_type("int32");
    bar.set_model(FieldModel::Simple);

    EntSerializer mode_a;
    mode_a.name = "ModeA";
    mode_a.add_field(&foo);

    EntSerializer mode_b;
    mode_b.name = "ModeB";
    mode_b.add_field(&bar);

    EntField mode_ptr;
    mode_ptr.var_name = "m_pMode";
    mode_ptr.var_type = "ModeA*";
    mode_ptr.type = parse_field_type("ModeA*");
    mode_ptr.serializer = &mode_a;
    mode_ptr.poly_types = {&mode_a, &mode_b};
    mode_ptr.poly_serializer_id = 0;
    mode_ptr.set_model(FieldModel::FixedTable);
    CYKA_CHECK(mode_ptr.base_decoder.op == DecOp::PolyBase);

    EntSerializer root;
    root.name = "Root";
    root.add_field(&mode_ptr);

    FieldPath fp;
    fp.path[0] = 0;
    fp.path[1] = 0;
    fp.last = 1;

    const std::vector<const EntSerializer*> poly_a{&mode_a};
    const std::vector<const EntSerializer*> poly_b{&mode_b};
    const std::vector<const EntSerializer*> poly_off{nullptr};

    const auto sel_a = root.select(fp, 0, poly_a);
    const auto sel_b = root.select(fp, 0, poly_b);
    const auto sel_off = root.select(fp, 0, poly_off);
    const auto sel_def = root.select(fp, 0, {});

    CYKA_CHECK(sel_a.ok && sel_a.spec == &foo.decoder);
    CYKA_CHECK(sel_b.ok && sel_b.spec == &bar.decoder);
    CYKA_CHECK(!sel_off.ok);
    CYKA_CHECK(sel_def.ok && sel_def.spec == &foo.decoder);

    CYKA_CHECK(root.max_poly_id() == 0);
}

} // namespace

void test_ent_decode() {
    test_qangle_noscale_32();
    test_qangle_scaled_20();
    test_poly_wire_inactive_skips_ubitvar();
    test_poly_wire_active_index();
    test_poly_per_entity_select();
}
