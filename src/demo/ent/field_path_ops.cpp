// Field-path delta operations, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_path.go. Table order and weights are
// load-bearing: they define the huffman code. See NOTICE.

#include "cyka/demo/ent/field_path_ops.hpp"

namespace cyka::demo::ent {
namespace {

std::int32_t& top(FieldPath& fp) noexcept {
    return fp.path[static_cast<std::size_t>(fp.last)];
}

std::int32_t fpv(BitStream& r) noexcept {
    return static_cast<std::int32_t>(r.read_ubit_var_fp());
}

std::int32_t bits(BitStream& r, std::uint32_t n) noexcept {
    return static_cast<std::int32_t>(r.read_bits(n));
}

void push_set(FieldPath& fp, std::int32_t v) noexcept {
    fp.push();
    top(fp) = v;
}

void push_add(FieldPath& fp, std::int32_t v) noexcept {
    fp.push();
    top(fp) += v;
}

void non_topo(BitStream& r, FieldPath& fp, int delta_bias, bool pack4) noexcept {
    for (int i = 0; i <= fp.last; ++i) {
        if (r.read_bool()) {
            fp.path[static_cast<std::size_t>(i)] +=
                pack4 ? bits(r, 4) - 7 : r.read_var_i32() + delta_bias;
        }
    }
}

} // namespace

// clang-format off
const std::array<FieldPathOp, kFieldPathOpCount> kFieldPathOps{{
    {36271, [](BitStream&, FieldPath& fp) { top(fp) += 1; }},
    {10334, [](BitStream&, FieldPath& fp) { top(fp) += 2; }},
    {1375,  [](BitStream&, FieldPath& fp) { top(fp) += 3; }},
    {646,   [](BitStream&, FieldPath& fp) { top(fp) += 4; }},
    {4128,  [](BitStream& r, FieldPath& fp) { top(fp) += fpv(r) + 5; }},
    {35,    [](BitStream&, FieldPath& fp) { push_set(fp, 0); }},
    {3,     [](BitStream& r, FieldPath& fp) { push_set(fp, fpv(r)); }},
    {521,   [](BitStream&, FieldPath& fp) { top(fp) += 1; push_set(fp, 0); }},
    {2942,  [](BitStream& r, FieldPath& fp) { top(fp) += 1; push_set(fp, fpv(r)); }},
    {560,   [](BitStream& r, FieldPath& fp) { top(fp) += fpv(r); push_set(fp, 0); }},
    {471,   [](BitStream& r, FieldPath& fp) { top(fp) += fpv(r) + 2; push_set(fp, fpv(r) + 1); }},
    {10530, [](BitStream& r, FieldPath& fp) { top(fp) += bits(r, 3) + 2; push_set(fp, bits(r, 3) + 1); }},
    {251,   [](BitStream& r, FieldPath& fp) { top(fp) += bits(r, 4) + 2; push_set(fp, bits(r, 4) + 1); }},
    {0,     [](BitStream& r, FieldPath& fp) { push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { push_set(fp, bits(r, 5)); push_set(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) { push_add(fp, fpv(r)); push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { push_set(fp, bits(r, 5)); push_set(fp, bits(r, 5)); push_set(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += 1; push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += 1; push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += 1; push_add(fp, fpv(r)); push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += 1; push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += static_cast<std::int32_t>(r.read_ubit_var()) + 2; push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += static_cast<std::int32_t>(r.read_ubit_var()) + 2; push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += static_cast<std::int32_t>(r.read_ubit_var()) + 2; push_add(fp, fpv(r)); push_add(fp, fpv(r)); push_add(fp, fpv(r)); }},
    {0,     [](BitStream& r, FieldPath& fp) { top(fp) += static_cast<std::int32_t>(r.read_ubit_var()) + 2; push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); push_add(fp, bits(r, 5)); }},
    {0,     [](BitStream& r, FieldPath& fp) {
                const auto n = static_cast<int>(r.read_ubit_var());
                top(fp) += static_cast<std::int32_t>(r.read_ubit_var());
                for (int i = 0; i < n; ++i) { push_add(fp, fpv(r)); }
            }},
    {310,   [](BitStream& r, FieldPath& fp) {
                for (int i = 0; i <= fp.last; ++i) {
                    if (r.read_bool()) { fp.path[static_cast<std::size_t>(i)] += r.read_var_i32() + 1; }
                }
                const auto count = static_cast<int>(r.read_ubit_var());
                for (int i = 0; i < count; ++i) { push_set(fp, fpv(r)); }
            }},
    {2,     [](BitStream&, FieldPath& fp) { fp.pop(1); top(fp) += 1; }},
    {0,     [](BitStream& r, FieldPath& fp) { fp.pop(1); top(fp) += fpv(r) + 1; }},
    {1837,  [](BitStream&, FieldPath& fp) { fp.pop(fp.last); fp.path[0] += 1; }},
    {149,   [](BitStream& r, FieldPath& fp) { fp.pop(fp.last); fp.path[0] += fpv(r) + 1; }},
    {300,   [](BitStream& r, FieldPath& fp) { fp.pop(fp.last); fp.path[0] += bits(r, 3) + 1; }},
    {634,   [](BitStream& r, FieldPath& fp) { fp.pop(fp.last); fp.path[0] += bits(r, 6) + 1; }},
    {0,     [](BitStream& r, FieldPath& fp) { fp.pop(fpv(r)); top(fp) += 1; }},
    {0,     [](BitStream& r, FieldPath& fp) { fp.pop(fpv(r)); top(fp) += r.read_var_i32(); }},
    {1,     [](BitStream& r, FieldPath& fp) { fp.pop(fpv(r)); non_topo(r, fp, 0, false); }},
    {76,    [](BitStream& r, FieldPath& fp) { non_topo(r, fp, 0, false); }},
    {271,   [](BitStream&, FieldPath& fp) {
                if (fp.last > 0) { fp.path[static_cast<std::size_t>(fp.last - 1)] += 1; }
            }},
    {99,    [](BitStream& r, FieldPath& fp) { non_topo(r, fp, 0, true); }},
    {25474, [](BitStream&, FieldPath& fp) { fp.done = true; }},
}};
// clang-format on

} // namespace cyka::demo::ent
