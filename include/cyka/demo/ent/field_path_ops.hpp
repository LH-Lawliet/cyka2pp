#pragma once

#include "cyka/demo/ent/field_path.hpp"

#include <array>
#include <cstdint>

namespace cyka::demo::ent {

inline constexpr std::size_t kFieldPathOpCount = 40;

struct FieldPathOp {
    int weight{0};
    void (*fn)(BitStream&, FieldPath&){nullptr};
};

/// Order matches demoinfocs' fieldPathTable; the weights build the huffman code.
extern const std::array<FieldPathOp, kFieldPathOpCount> kFieldPathOps;

} // namespace cyka::demo::ent
