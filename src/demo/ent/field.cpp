// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/{field,field_type}.go.
// See NOTICE.

#include "cyka/demo/ent/field.hpp"


namespace cyka::demo::ent {

void EntField::set_model(FieldModel m) {
    model = m;
    switch (m) {
    case FieldModel::FixedArray:
    case FieldModel::Simple:
        decoder = find_decoder(*this);
        break;
    case FieldModel::FixedTable:
        base_decoder = DecoderSpec{};
        base_decoder.op = poly_types.empty() ? DecOp::Bool : DecOp::PolyBase;
        break;
    case FieldModel::VariableArray:
        base_decoder = DecoderSpec{};
        base_decoder.op = DecOp::Unsigned;
        child_decoder = find_decoder_by_base(*this);
        break;
    case FieldModel::VariableTable:
        base_decoder = DecoderSpec{};
        base_decoder.op = DecOp::Unsigned;
        break;
    }
}

void EntField::apply_poly(std::uint64_t index) const {
    if (index >= 1 && index <= poly_types.size()) {
        serializer = poly_types[static_cast<std::size_t>(index - 1)];
    }
}

DecodeSel EntField::select(const FieldPath& fp, int pos) const {
    switch (model) {
    case FieldModel::FixedArray:
        return {&decoder, this, false, true};
    case FieldModel::FixedTable:
        if (fp.last == pos - 1) {
            return {&base_decoder, this, false, true};
        }
        return serializer != nullptr ? serializer->select(fp, pos) : DecodeSel{};
    case FieldModel::VariableArray:
        if (fp.last == pos) {
            return {&child_decoder, this, false, true};
        }
        return {&base_decoder, this, true, true};
    case FieldModel::VariableTable:
        if (fp.last >= pos + 1) {
            return serializer != nullptr ? serializer->select(fp, pos + 1) : DecodeSel{};
        }
        return {&base_decoder, this, true, true};
    case FieldModel::Simple:
    default:
        return {&decoder, this, false, true};
    }
}

bool EntField::path_for_name(FieldPath& fp, std::string_view name) const {
    switch (model) {
    case FieldModel::FixedArray:
    case FieldModel::VariableArray: {
        const auto n = parse_path_index(name);
        if (!n) {
            return false;
        }
        fp.path[static_cast<std::size_t>(fp.last)] = *n;
        return true;
    }
    case FieldModel::FixedTable:
        return serializer != nullptr && serializer->path_for_name(fp, name);
    case FieldModel::VariableTable: {
        if (name.size() < 6) {
            return false;
        }
        const auto n = parse_path_index(name.substr(0, 4));
        if (!n) {
            return false;
        }
        fp.path[static_cast<std::size_t>(fp.last)] = *n;
        fp.push();
        return serializer != nullptr && serializer->path_for_name(fp, name.substr(5));
    }
    case FieldModel::Simple:
    default:
        return false;
    }
}

void EntSerializer::add_field(const EntField* f) {
    index_by_name[f->var_name] = fields.size();
    fields.push_back(f);
}

DecodeSel EntSerializer::select(const FieldPath& fp, int pos) const {
    if (pos < 0 || pos > fp.last) {
        return {};
    }
    const auto idx = fp.path[static_cast<std::size_t>(pos)];
    if (idx < 0 || static_cast<std::size_t>(idx) >= fields.size()) {
        return {};
    }
    return fields[static_cast<std::size_t>(idx)]->select(fp, pos + 1);
}

bool EntSerializer::path_for_name(FieldPath& fp, std::string_view name) const {
    if (const auto it = index_by_name.find(std::string{name}); it != index_by_name.end()) {
        fp.path[static_cast<std::size_t>(fp.last)] = static_cast<std::int32_t>(it->second);
        return true;
    }
    const auto dot = name.find('.');
    if (dot == std::string_view::npos) {
        return false;
    }
    const auto it = index_by_name.find(std::string{name.substr(0, dot)});
    if (it == index_by_name.end()) {
        return false;
    }
    fp.path[static_cast<std::size_t>(fp.last)] = static_cast<std::int32_t>(it->second);
    fp.push();
    const auto* f = fields[it->second];
    return f->path_for_name(fp, name.substr(f->var_name.size() + 1));
}

std::optional<std::uint64_t> EntClass::key_for(const std::string& name) const {
    if (const auto it = key_cache.find(name); it != key_cache.end()) {
        return it->second;
    }
    std::optional<std::uint64_t> key;
    FieldPath fp;
    if (serializer != nullptr && serializer->path_for_name(fp, name)) {
        key = fp.key();
    }
    key_cache.emplace(name, key);
    return key;
}

} // namespace cyka::demo::ent
