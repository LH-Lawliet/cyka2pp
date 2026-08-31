#include "cyka/demo/ent/decoder.hpp"
#include "cyka/demo/ent/field.hpp"
#include "test_harness.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <vector>

namespace {

using cyka::demo::ent::BitStream;
using cyka::demo::ent::DecoderSpec;
using cyka::demo::ent::decodeValue;
using cyka::demo::ent::DecOp;
using cyka::demo::ent::EntField;
using cyka::demo::ent::EntSerializer;
using cyka::demo::ent::FieldModel;
using cyka::demo::ent::FieldPath;
using cyka::demo::ent::findDecoder;
using cyka::demo::ent::parseFieldType;

inline constexpr int F32_BYTES = 4;
inline constexpr int BYTE_SHIFT = 8;
inline constexpr int QANGLE_BITS = 20;
inline constexpr int QANGLE_BITS_FULL = 32;
inline constexpr int QANGLE_COMPONENTS = 3;
inline constexpr float TEST_ANGLE_X = 1.5F;
inline constexpr float TEST_ANGLE_Y = -4.25F;
inline constexpr float TEST_ANGLE_Z = 350.0F;
inline constexpr float FULL_CIRCLE_DEG = 360.0F;

void appendF32Le(std::vector<std::uint8_t>& out, float value) {
    const auto BITS = std::bit_cast<std::uint32_t>(value);
    for (int byte_idx = 0; byte_idx < F32_BYTES; ++byte_idx) {
        const unsigned SHIFT = static_cast<unsigned>(BYTE_SHIFT) * static_cast<unsigned>(byte_idx);
        out.push_back(static_cast<std::uint8_t>(BITS >> SHIFT));
    }
}

void testQangleNoscale32() {
    EntField field;
    field.var_type = "QAngle";
    field.type = parseFieldType("QAngle");
    field.bit_count = QANGLE_BITS_FULL;
    const DecoderSpec SPEC = findDecoder(field);
    CYKA_CHECK(SPEC.op == DecOp::VECTOR);
    CYKA_CHECK(SPEC.sub == DecOp::NO_SCALE);
    CYKA_CHECK(SPEC.comps == QANGLE_COMPONENTS);

    std::vector<std::uint8_t> raw;
    appendF32Le(raw, TEST_ANGLE_X);
    appendF32Le(raw, TEST_ANGLE_Y);
    appendF32Le(raw, TEST_ANGLE_Z);
    BitStream reader(raw);
    const auto VALUE = decodeValue(SPEC, reader);
    CYKA_CHECK(!reader.failed());
    CYKA_CHECK(VALUE.v3[0] == TEST_ANGLE_X);
    CYKA_CHECK(VALUE.v3[1] == TEST_ANGLE_Y);
    CYKA_CHECK(VALUE.v3[2] == TEST_ANGLE_Z);
}

void testQangleScaled20() {
    EntField field;
    field.var_type = "QAngle";
    field.type = parseFieldType("QAngle");
    field.bit_count = QANGLE_BITS;
    const DecoderSpec SPEC = findDecoder(field);
    CYKA_CHECK(SPEC.op == DecOp::Q_ANGLE_BITS);
    CYKA_CHECK(SPEC.bits == QANGLE_BITS);

    // 20-bit 0x80000 maps to 180 degrees; two more components follow.
    const std::array<std::uint8_t, 8> RAW{0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00};
    BitStream reader(RAW);
    const auto VALUE = decodeValue(SPEC, reader);
    CYKA_CHECK(VALUE.v3[0] >= 0.0F && VALUE.v3[0] < FULL_CIRCLE_DEG);
    CYKA_CHECK(VALUE.v3[1] >= 0.0F && VALUE.v3[1] < FULL_CIRCLE_DEG);
    CYKA_CHECK(VALUE.v3[2] >= 0.0F && VALUE.v3[2] < FULL_CIRCLE_DEG);
}

void testPolyWireInactiveSkipsUbitvar() {
    DecoderSpec spec;
    spec.op = DecOp::POLY_BASE;
    // bit0 = 0 (inactive), bit1 = 1. Old decoder would swallow bit1 as ubitvar.
    const std::array<std::uint8_t, 1> RAW{0x02};
    BitStream reader(RAW);
    const auto VALUE = decodeValue(spec, reader);
    CYKA_CHECK(!VALUE.b);
    CYKA_CHECK(reader.readBool());
    CYKA_CHECK(!reader.failed());
}

void testPolyWireActiveIndex() {
    DecoderSpec spec;
    spec.op = DecOp::POLY_BASE;
    {
        const std::array<std::uint8_t, 1> RAW{0x01}; // active, index 0
        BitStream reader(RAW);
        const auto VALUE = decodeValue(spec, reader);
        CYKA_CHECK(VALUE.b);
        CYKA_CHECK(VALUE.u == 0);
    }
    {
        const std::array<std::uint8_t, 1> RAW{0x03}; // active, index 1
        BitStream reader(RAW);
        const auto VALUE = decodeValue(spec, reader);
        CYKA_CHECK(VALUE.b);
        CYKA_CHECK(VALUE.u == 1);
    }
}

void testPolyPerEntitySelect() {
    EntField foo;
    foo.var_name = "m_nFoo";
    foo.var_type = "uint32";
    foo.type = parseFieldType("uint32");
    foo.setModel(FieldModel::SIMPLE);

    EntField bar;
    bar.var_name = "m_nBar";
    bar.var_type = "int32";
    bar.type = parseFieldType("int32");
    bar.setModel(FieldModel::SIMPLE);

    EntSerializer mode_a;
    mode_a.name = "ModeA";
    mode_a.addField(&foo);

    EntSerializer mode_b;
    mode_b.name = "ModeB";
    mode_b.addField(&bar);

    EntField mode_ptr;
    mode_ptr.var_name = "m_pMode";
    mode_ptr.var_type = "ModeA*";
    mode_ptr.type = parseFieldType("ModeA*");
    mode_ptr.serializer = &mode_a;
    mode_ptr.poly_types = {&mode_a, &mode_b};
    mode_ptr.poly_serializer_id = 0;
    mode_ptr.setModel(FieldModel::FIXED_TABLE);
    CYKA_CHECK(mode_ptr.base_decoder.op == DecOp::POLY_BASE);

    EntSerializer root;
    root.name = "Root";
    root.addField(&mode_ptr);

    FieldPath field_path;
    field_path.path[0] = 0;
    field_path.path[1] = 0;
    field_path.last = 1;

    const std::vector<const EntSerializer*> POLY_A{&mode_a};
    const std::vector<const EntSerializer*> POLY_B{&mode_b};
    const std::vector<const EntSerializer*> POLY_OFF{nullptr};

    const auto SEL_A = root.select(field_path, 0, POLY_A);
    const auto SEL_B = root.select(field_path, 0, POLY_B);
    const auto SEL_OFF = root.select(field_path, 0, POLY_OFF);
    const auto SEL_DEF = root.select(field_path, 0, {});

    CYKA_CHECK(SEL_A.ok && SEL_A.spec == &foo.decoder);
    CYKA_CHECK(SEL_B.ok && SEL_B.spec == &bar.decoder);
    CYKA_CHECK(!SEL_OFF.ok);
    CYKA_CHECK(SEL_DEF.ok && SEL_DEF.spec == &foo.decoder);

    CYKA_CHECK(root.maxPolyId() == 0);
}

} // namespace

void test_ent_decode() {
    testQangleNoscale32();
    testQangleScaled20();
    testPolyWireInactiveSkipsUbitvar();
    testPolyWireActiveIndex();
    testPolyPerEntitySelect();
}
