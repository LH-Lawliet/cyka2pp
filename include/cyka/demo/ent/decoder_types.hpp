#pragma once

#include "cyka/demo/ent/decoder.hpp"

#include <string_view>
#include <unordered_map>

namespace cyka::demo::ent {

/// Base send-table type name → fixed decoder op (demoinfocs fieldTypeDecoders).
/// Types whose decoder depends on the field's encoder live in decoder_select.
[[nodiscard]] const std::unordered_map<std::string_view, DecOp>& typeDecoders();

} // namespace cyka::demo::ent
