#pragma once

#include "cyka/demo/ent/field_path.hpp"

#include <array>

namespace cyka::demo::ent {

inline constexpr std::size_t FIELD_PATH_OP_COUNT = 40;

struct FieldPathOp {
    int weight{0};
    void (*fn)(BitStream&, FieldPath&){nullptr};
};

/// Order matches demoinfocs' fieldPathTable; the weights build the huffman code.
extern const std::array<FieldPathOp, FIELD_PATH_OP_COUNT> FIELD_PATH_OPS;

} // namespace cyka::demo::ent
