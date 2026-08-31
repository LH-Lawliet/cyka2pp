// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/{field,field_type}.go.
// See NOTICE.

#include "cyka/demo/ent/field.hpp"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace cyka::demo::ent {
namespace {

inline constexpr std::size_t VAR_TABLE_PREFIX_LEN = 4;
inline constexpr std::size_t VAR_TABLE_NAME_OFFSET = 5;
inline constexpr std::size_t MIN_VAR_TABLE_NAME_LEN = 6;

const EntSerializer* boundSerializer(const EntField& field, PolyView poly) {
    if (field.poly_serializer_id >= 0) {
        if (static_cast<std::size_t>(field.poly_serializer_id) < poly.size()) {
            return poly[static_cast<std::size_t>(field.poly_serializer_id)];
        }
        if (poly.empty()) {
            return field.serializer;
        }
        return nullptr;
    }
    return field.serializer;
}

int walkMaxPoly(const EntSerializer* root, std::unordered_set<const EntSerializer*>& seen) {
    int max_id = -1;
    std::vector<const EntSerializer*> stack;
    if (root != nullptr) {
        stack.push_back(root);
    }
    while (!stack.empty()) {
        const EntSerializer* ser = stack.back();
        stack.pop_back();
        if (ser == nullptr || !seen.insert(ser).second) {
            continue;
        }
        for (const auto* field : ser->fields) {
            if (field->poly_serializer_id >= 0) {
                max_id = std::max(max_id, field->poly_serializer_id);
            }
            if (field->serializer != nullptr) {
                stack.push_back(field->serializer);
            }
            for (const auto* poly_type : field->poly_types) {
                if (poly_type != nullptr) {
                    stack.push_back(poly_type);
                }
            }
        }
    }
    return max_id;
}

} // namespace

void EntField::setModel(FieldModel model_val) {
    model = model_val;
    switch (model_val) {
    case FieldModel::FIXED_ARRAY:
    case FieldModel::SIMPLE:
        decoder = findDecoder(*this);
        break;
    case FieldModel::FIXED_TABLE:
        base_decoder = DecoderSpec{};
        base_decoder.op = poly_types.empty() ? DecOp::BOOL : DecOp::POLY_BASE;
        break;
    case FieldModel::VARIABLE_ARRAY:
        base_decoder = DecoderSpec{};
        base_decoder.op = DecOp::UNSIGNED;
        child_decoder = findDecoderByBase(*this);
        break;
    case FieldModel::VARIABLE_TABLE:
        base_decoder = DecoderSpec{};
        base_decoder.op = DecOp::UNSIGNED;
        break;
    }
}

DecodeSel EntField::select(const FieldPath& field_path, int pos, PolyView poly) const {
    const EntField* field = this;
    const EntSerializer* ser = nullptr;
    while (true) {
        if (ser != nullptr) {
            if (pos < 0 || pos > field_path.last) {
                return {};
            }
            const auto IDX = field_path.path[static_cast<std::size_t>(pos)];
            if (IDX < 0 || static_cast<std::size_t>(IDX) >= ser->fields.size()) {
                return {};
            }
            field = ser->fields[static_cast<std::size_t>(IDX)];
            ser = nullptr;
            ++pos;
            continue;
        }
        switch (field->model) {
        case FieldModel::FIXED_ARRAY:
            return {.spec = &field->decoder, .field = field, .collection = false, .ok = true};
        case FieldModel::FIXED_TABLE:
            if (field_path.last == pos - 1) {
                return {
                    .spec = &field->base_decoder, .field = field, .collection = false, .ok = true};
            }
            ser = boundSerializer(*field, poly);
            if (ser == nullptr) {
                return {};
            }
            continue;
        case FieldModel::VARIABLE_ARRAY:
            if (field_path.last == pos) {
                return {
                    .spec = &field->child_decoder, .field = field, .collection = false, .ok = true};
            }
            return {.spec = &field->base_decoder, .field = field, .collection = true, .ok = true};
        case FieldModel::VARIABLE_TABLE:
            if (field_path.last >= pos + 1) {
                ser = field->serializer;
                if (ser == nullptr) {
                    return {};
                }
                ++pos;
                continue;
            }
            return {.spec = &field->base_decoder, .field = field, .collection = true, .ok = true};
        case FieldModel::SIMPLE:
        default:
            return {.spec = &field->decoder, .field = field, .collection = false, .ok = true};
        }
    }
}

bool EntField::pathForName(FieldPath& field_path, std::string_view name, PolyView poly) const {
    switch (model) {
    case FieldModel::FIXED_ARRAY:
    case FieldModel::VARIABLE_ARRAY: {
        const auto IDX = parsePathIndex(name);
        if (!IDX) {
            return false;
        }
        field_path.path[static_cast<std::size_t>(field_path.last)] = *IDX;
        return true;
    }
    case FieldModel::FIXED_TABLE: {
        const EntSerializer* ser = boundSerializer(*this, poly);
        return ser != nullptr && ser->pathForName(field_path, name, poly);
    }
    case FieldModel::VARIABLE_TABLE: {
        if (name.size() < MIN_VAR_TABLE_NAME_LEN) {
            return false;
        }
        const auto IDX = parsePathIndex(name.substr(0, VAR_TABLE_PREFIX_LEN));
        if (!IDX) {
            return false;
        }
        field_path.path[static_cast<std::size_t>(field_path.last)] = *IDX;
        field_path.push();
        return serializer != nullptr &&
               serializer->pathForName(field_path, name.substr(VAR_TABLE_NAME_OFFSET), poly);
    }
    case FieldModel::SIMPLE:
    default:
        return false;
    }
}

void EntSerializer::addField(const EntField* field) {
    index_by_name[field->var_name] = fields.size();
    fields.push_back(field);
}

DecodeSel EntSerializer::select(const FieldPath& field_path, int pos, PolyView poly) const {
    if (pos < 0 || pos > field_path.last) {
        return {};
    }
    const auto IDX = field_path.path[static_cast<std::size_t>(pos)];
    if (IDX < 0 || static_cast<std::size_t>(IDX) >= fields.size()) {
        return {};
    }
    return fields[static_cast<std::size_t>(IDX)]->select(field_path, pos + 1, poly);
}

bool EntSerializer::pathForName(FieldPath& field_path, std::string_view name, PolyView poly) const {
    const EntSerializer* ser = this;
    std::string_view remaining = name;
    while (ser != nullptr) {
        const auto DOT = remaining.find('.');
        const std::string_view SEGMENT =
            DOT == std::string_view::npos ? remaining : remaining.substr(0, DOT);
        const auto ITER = ser->index_by_name.find(std::string{SEGMENT});
        if (ITER == ser->index_by_name.end()) {
            return false;
        }
        field_path.path[static_cast<std::size_t>(field_path.last)] =
            static_cast<std::int32_t>(ITER->second);
        if (DOT == std::string_view::npos) {
            return true;
        }
        field_path.push();
        const EntField* field = ser->fields[ITER->second];
        remaining = remaining.substr(field->var_name.size() + 1);
        switch (field->model) {
        case FieldModel::FIXED_TABLE:
            ser = boundSerializer(*field, poly);
            break;
        case FieldModel::VARIABLE_TABLE: {
            if (remaining.size() < MIN_VAR_TABLE_NAME_LEN) {
                return false;
            }
            const auto IDX = parsePathIndex(remaining.substr(0, VAR_TABLE_PREFIX_LEN));
            if (!IDX) {
                return false;
            }
            field_path.path[static_cast<std::size_t>(field_path.last)] = *IDX;
            field_path.push();
            ser = field->serializer;
            remaining = remaining.substr(VAR_TABLE_NAME_OFFSET);
            break;
        }
        case FieldModel::FIXED_ARRAY:
        case FieldModel::VARIABLE_ARRAY: {
            // Inline leaf array handling to avoid a pathForName call cycle with EntField.
            const auto IDX = parsePathIndex(remaining);
            if (!IDX) {
                return false;
            }
            field_path.path[static_cast<std::size_t>(field_path.last)] = *IDX;
            return true;
        }
        case FieldModel::SIMPLE:
        default:
            return false;
        }
    }
    return false;
}

int EntSerializer::maxPolyId() const {
    std::unordered_set<const EntSerializer*> seen;
    return walkMaxPoly(this, seen);
}

std::optional<std::uint64_t> EntClass::keyFor(const std::string& name) const {
    if (const auto ITER = key_cache.find(name); ITER != key_cache.end()) {
        return ITER->second;
    }
    std::optional<std::uint64_t> key;
    FieldPath field_path;
    if (serializer != nullptr && serializer->pathForName(field_path, name)) {
        key = field_path.key();
    }
    key_cache.emplace(name, key);
    return key;
}

} // namespace cyka::demo::ent
