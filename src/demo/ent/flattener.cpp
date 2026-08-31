// CSVCMsg_FlattenedSerializer ingestion, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/parser.go (ParsePacket / newField). See NOTICE.

#include "cyka/demo/ent/context.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <bit>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::kWire32;
using cyka::demo::kWireLen;
using cyka::demo::kWireVarint;

float float_field(std::span<const std::uint8_t> b) {
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < 4 && i < b.size(); ++i) {
        v |= static_cast<std::uint32_t>(b[i]) << (8 * i);
    }
    return std::bit_cast<float>(v);
}

/// ProtoFlattenedSerializer_t.fields_index (field 3), packed or unpacked.
void collect_indices(std::span<const std::uint8_t> msg, std::vector<std::int32_t>& out) {
    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        if (f->field != 3) {
            continue;
        }
        if (f->wire == kWireVarint) {
            out.push_back(static_cast<std::int32_t>(f->varint));
        } else if (f->wire == kWireLen) {
            ByteReader pr(f->bytes);
            while (auto v = pr.read_varint_u64()) {
                out.push_back(static_cast<std::int32_t>(*v));
            }
        }
    }
}

} // namespace

EntField* EntityContext::make_field(std::span<const std::uint8_t> msg,
                                    const std::vector<std::string>& symbols) {
    auto owned = std::make_unique<EntField>();
    EntField* f = owned.get();
    const auto sym = [&](std::uint64_t idx) -> std::string {
        return idx < symbols.size() ? symbols[static_cast<std::size_t>(idx)] : std::string{};
    };

    ByteReader r(msg);
    std::vector<std::string> poly_names;
    while (auto fld = cyka::demo::read_field(r)) {
        switch (fld->field) {
        case 1:
            f->var_type = sym(fld->varint);
            break;
        case 2:
            f->var_name = sym(fld->varint);
            break;
        case 3:
            f->bit_count = static_cast<std::int32_t>(fld->varint);
            break;
        case 4:
            if (fld->wire == kWire32) {
                f->low_value = float_field(fld->bytes);
            }
            break;
        case 5:
            if (fld->wire == kWire32) {
                f->high_value = float_field(fld->bytes);
            }
            break;
        case 6:
            f->encode_flags = static_cast<std::int32_t>(fld->varint);
            break;
        case 7:
            f->serializer_name = sym(fld->varint);
            break;
        case 9:
            f->send_node = sym(fld->varint);
            break;
        case 10:
            f->encoder = sym(fld->varint);
            break;
        case 11:
            if (fld->wire == kWireLen) {
                ByteReader pr(fld->bytes);
                while (auto pf = cyka::demo::read_field(pr)) {
                    if (pf->field == 1 && pf->wire == kWireVarint) {
                        poly_names.push_back(sym(pf->varint));
                    }
                }
            }
            break;
        default:
            break;
        }
    }
    if (f->send_node == "(root)") {
        f->send_node.clear();
    }
    // demoinfocs fieldPatches: these two are networked as tick counts.
    if (f->var_name == "m_flSimulationTime" || f->var_name == "m_flAnimTime") {
        f->encoder = "simtime";
    }
    f->type = parse_field_type(f->var_type);
    if (!f->serializer_name.empty()) {
        f->serializer = serializer_for(f->serializer_name);
    }
    if (!poly_names.empty()) {
        // Combined slice: [0] = default serializer, [1..N] = alternatives.
        // The ubitvar on the wire is a direct index into this slice.
        f->poly_types.resize(poly_names.size() + 1);
        f->poly_types[0] = f->serializer;
        for (std::size_t i = 0; i < poly_names.size(); ++i) {
            f->poly_types[i + 1] = serializer_for(poly_names[i]);
        }
        f->poly_serializer_id = next_poly_id_++;
    }

    FieldModel model = FieldModel::Simple;
    if (f->serializer != nullptr || !f->poly_types.empty()) {
        if ((f->type != nullptr && (f->type->pointer || is_pointer_type(f->type->base))) ||
            !f->poly_types.empty()) {
            model = FieldModel::FixedTable;
        } else {
            model = FieldModel::VariableTable;
        }
    } else if (f->type->count > 0 && f->type->base != "char") {
        model = FieldModel::FixedArray;
    } else if (f->type->base == "CUtlVector" || f->type->base == "CNetworkUtlVectorBase") {
        model = FieldModel::VariableArray;
    }
    f->set_model(model);

    field_pool_.push_back(std::move(owned));
    return f;
}

void EntityContext::load_flattened(std::span<const std::uint8_t> msg) {
    std::vector<std::string> symbols;
    std::vector<std::span<const std::uint8_t>> field_msgs;
    std::vector<std::span<const std::uint8_t>> serializer_msgs;

    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        if (f->wire != kWireLen) {
            continue;
        }
        if (f->field == 1) {
            serializer_msgs.push_back(f->bytes);
        } else if (f->field == 2) {
            symbols.emplace_back(cyka::demo::as_string(f->bytes));
        } else if (f->field == 3) {
            field_msgs.push_back(f->bytes);
        }
    }

    std::unordered_map<std::int32_t, EntField*> by_index;
    std::vector<std::int32_t> indices;
    for (const auto& sm : serializer_msgs) {
        auto ser = std::make_unique<EntSerializer>();
        ByteReader sr(sm);
        while (auto f = cyka::demo::read_field(sr)) {
            if (f->field == 1 && f->wire == kWireVarint) {
                const auto idx = static_cast<std::size_t>(f->varint);
                ser->name = idx < symbols.size() ? symbols[idx] : std::string{};
            } else if (f->field == 2 && f->wire == kWireVarint) {
                ser->version = static_cast<std::int32_t>(f->varint);
            }
        }
        indices.clear();
        collect_indices(sm, indices);
        for (const auto i : indices) {
            if (i < 0 || static_cast<std::size_t>(i) >= field_msgs.size()) {
                continue;
            }
            auto it = by_index.find(i);
            if (it == by_index.end()) {
                it = by_index
                         .emplace(i, make_field(field_msgs[static_cast<std::size_t>(i)], symbols))
                         .first;
            }
            ser->add_field(it->second);
        }
        EntSerializer* raw = ser.get();
        serializer_pool_.push_back(std::move(ser));
        serializers_[raw->name] = raw;
        if (const auto ci = classes_by_name_.find(raw->name); ci != classes_by_name_.end()) {
            ci->second->serializer = raw;
            bind_poly_count(ci->second);
        }
    }
}

} // namespace cyka::demo::ent
