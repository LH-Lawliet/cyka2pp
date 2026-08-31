// Field-path delta operations, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_path.go. Table order and weights are
// load-bearing: they define the huffman code. See NOTICE.

#include "cyka/demo/ent/field_path_ops.hpp"

namespace cyka::demo::ent {
namespace {

inline constexpr std::int32_t FP_DELTA_2 = 2;
inline constexpr std::int32_t FP_DELTA_3 = 3;
inline constexpr std::int32_t FP_DELTA_4 = 4;
inline constexpr std::int32_t FP_DELTA_5 = 5;
inline constexpr std::int32_t FP_PACK4_BIAS = 7;
inline constexpr std::uint32_t FP_BITS_3 = 3;
inline constexpr std::uint32_t FP_BITS_4 = 4;
inline constexpr std::uint32_t FP_BITS_5 = 5;
inline constexpr std::uint32_t FP_BITS_6 = 6;

// Weights are verbatim from demoinfocs; numeric literals are load-bearing.
inline constexpr int FP_WT_36271 = 36271;
inline constexpr int FP_WT_10334 = 10334;
inline constexpr int FP_WT_1375 = 1375;
inline constexpr int FP_WT_646 = 646;
inline constexpr int FP_WT_4128 = 4128;
inline constexpr int FP_WT_35 = 35;
inline constexpr int FP_WT_3 = 3;
inline constexpr int FP_WT_521 = 521;
inline constexpr int FP_WT_2942 = 2942;
inline constexpr int FP_WT_560 = 560;
inline constexpr int FP_WT_471 = 471;
inline constexpr int FP_WT_10530 = 10530;
inline constexpr int FP_WT_251 = 251;
inline constexpr int FP_WT_310 = 310;
inline constexpr int FP_WT_2 = 2;
inline constexpr int FP_WT_1837 = 1837;
inline constexpr int FP_WT_149 = 149;
inline constexpr int FP_WT_300 = 300;
inline constexpr int FP_WT_634 = 634;
inline constexpr int FP_WT_76 = 76;
inline constexpr int FP_WT_271 = 271;
inline constexpr int FP_WT_99 = 99;
inline constexpr int FP_WT_25474 = 25474;

std::int32_t& top(FieldPath& field_path) noexcept {
    return field_path.path[static_cast<std::size_t>(field_path.last)];
}

std::int32_t fpv(BitStream& reader) noexcept {
    return static_cast<std::int32_t>(reader.readUbitVarFp());
}

std::int32_t bits(BitStream& reader, std::uint32_t num_bits) noexcept {
    return static_cast<std::int32_t>(reader.readBits(num_bits));
}

void pushSet(FieldPath& field_path, std::int32_t value) noexcept {
    field_path.push();
    top(field_path) = value;
}

void pushAdd(FieldPath& field_path, std::int32_t value) noexcept {
    field_path.push();
    top(field_path) += value;
}

void nonTopo(BitStream& reader, FieldPath& field_path, int delta_bias, bool pack4) noexcept {
    for (int idx = 0; idx <= field_path.last; ++idx) {
        if (reader.readBool()) {
            field_path.path[static_cast<std::size_t>(idx)] +=
                pack4 ? bits(reader, FP_BITS_4) - FP_PACK4_BIAS : reader.readVarI32() + delta_bias;
        }
    }
}

} // namespace

// clang-format off
// Weights are verbatim from demoinfocs; numeric literals are load-bearing.
const std::array<FieldPathOp, FIELD_PATH_OP_COUNT> FIELD_PATH_OPS{{
    {.weight=FP_WT_36271, .fn=[](BitStream&, FieldPath& field_path) { top(field_path) += 1; }},
    {.weight=FP_WT_10334, .fn=[](BitStream&, FieldPath& field_path) { top(field_path) += FP_DELTA_2; }},
    {.weight=FP_WT_1375,  .fn=[](BitStream&, FieldPath& field_path) { top(field_path) += FP_DELTA_3; }},
    {.weight=FP_WT_646,   .fn=[](BitStream&, FieldPath& field_path) { top(field_path) += FP_DELTA_4; }},
    {.weight=FP_WT_4128,  .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += fpv(reader) + FP_DELTA_5; }},
    {.weight=FP_WT_35,    .fn=[](BitStream&, FieldPath& field_path) { pushSet(field_path, 0); }},
    {.weight=FP_WT_3,     .fn=[](BitStream& reader, FieldPath& field_path) { pushSet(field_path, fpv(reader)); }},
    {.weight=FP_WT_521,   .fn=[](BitStream&, FieldPath& field_path) { top(field_path) += 1; pushSet(field_path, 0); }},
    {.weight=FP_WT_2942,  .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += 1; pushSet(field_path, fpv(reader)); }},
    {.weight=FP_WT_560,   .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += fpv(reader); pushSet(field_path, 0); }},
    {.weight=FP_WT_471,   .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += fpv(reader) + FP_DELTA_2; pushSet(field_path, fpv(reader) + 1); }},
    {.weight=FP_WT_10530, .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += bits(reader, FP_BITS_3) + FP_DELTA_2; pushSet(field_path, bits(reader, FP_BITS_3) + 1); }},
    {.weight=FP_WT_251,   .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += bits(reader, FP_BITS_4) + FP_DELTA_2; pushSet(field_path, bits(reader, FP_BITS_4) + 1); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { pushSet(field_path, bits(reader, FP_BITS_5)); pushSet(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { pushSet(field_path, bits(reader, FP_BITS_5)); pushSet(field_path, bits(reader, FP_BITS_5)); pushSet(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += 1; pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += 1; pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += 1; pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += 1; pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += static_cast<std::int32_t>(reader.readUbitVar()) + FP_DELTA_2; pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += static_cast<std::int32_t>(reader.readUbitVar()) + FP_DELTA_2; pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += static_cast<std::int32_t>(reader.readUbitVar()) + FP_DELTA_2; pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); pushAdd(field_path, fpv(reader)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { top(field_path) += static_cast<std::int32_t>(reader.readUbitVar()) + FP_DELTA_2; pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); pushAdd(field_path, bits(reader, FP_BITS_5)); }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) {
                const auto NUM = static_cast<int>(reader.readUbitVar());
                top(field_path) += static_cast<std::int32_t>(reader.readUbitVar());
                for (int idx = 0; idx < NUM; ++idx) { pushAdd(field_path, fpv(reader)); }
            }},
    {.weight=FP_WT_310,   .fn=[](BitStream& reader, FieldPath& field_path) {
                for (int idx = 0; idx <= field_path.last; ++idx) {
                    if (reader.readBool()) { field_path.path[static_cast<std::size_t>(idx)] += reader.readVarI32() + 1; }
                }
                const auto COUNT = static_cast<int>(reader.readUbitVar());
                for (int idx = 0; idx < COUNT; ++idx) { pushSet(field_path, fpv(reader)); }
            }},
    {.weight=FP_WT_2,     .fn=[](BitStream&, FieldPath& field_path) { field_path.pop(1); top(field_path) += 1; }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(1); top(field_path) += fpv(reader) + 1; }},
    {.weight=FP_WT_1837,  .fn=[](BitStream&, FieldPath& field_path) { field_path.pop(field_path.last); field_path.path[0] += 1; }},
    {.weight=FP_WT_149,   .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(field_path.last); field_path.path[0] += fpv(reader) + 1; }},
    {.weight=FP_WT_300,   .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(field_path.last); field_path.path[0] += bits(reader, FP_BITS_3) + 1; }},
    {.weight=FP_WT_634,   .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(field_path.last); field_path.path[0] += bits(reader, FP_BITS_6) + 1; }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(fpv(reader)); top(field_path) += 1; }},
    {.weight=0,     .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(fpv(reader)); top(field_path) += reader.readVarI32(); }},
    {.weight=1,     .fn=[](BitStream& reader, FieldPath& field_path) { field_path.pop(fpv(reader)); nonTopo(reader, field_path, 0, false); }},
    {.weight=FP_WT_76,    .fn=[](BitStream& reader, FieldPath& field_path) { nonTopo(reader, field_path, 0, false); }},
    {.weight=FP_WT_271,   .fn=[](BitStream&, FieldPath& field_path) {
                if (field_path.last > 0) { field_path.path[static_cast<std::size_t>(field_path.last - 1)] += 1; }
            }},
    {.weight=FP_WT_99,    .fn=[](BitStream& reader, FieldPath& field_path) { nonTopo(reader, field_path, 0, true); }},
    {.weight=FP_WT_25474, .fn=[](BitStream&, FieldPath& field_path) { field_path.done = true; }},
}};
// clang-format on

} // namespace cyka::demo::ent
