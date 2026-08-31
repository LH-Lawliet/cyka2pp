// CSVCMsg_FlattenedSerializer ingestion, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/parser.go (ParsePacket / newField). See NOTICE.

#include "cyka/demo/ent/context.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <bit>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::WIRE32;
using cyka::demo::WIRE_LEN;
using cyka::demo::WIRE_VARINT;

inline constexpr std::size_t FLOAT_BYTES = 4;
inline constexpr unsigned BYTE_SHIFT = 8U;
inline constexpr int PROTO_FIELD_VAR_TYPE = 1;
inline constexpr int PROTO_FIELD_VAR_NAME = 2;
inline constexpr int PROTO_FIELD_BIT_COUNT = 3;
inline constexpr int PROTO_FIELD_LOW_VALUE = 4;
inline constexpr int PROTO_FIELD_HIGH_VALUE = 5;
inline constexpr int PROTO_FIELD_ENCODE_FLAGS = 6;
inline constexpr int PROTO_FIELD_SERIALIZER = 7;
inline constexpr int PROTO_FIELD_SEND_NODE = 9;
inline constexpr int PROTO_FIELD_ENCODER = 10;
inline constexpr int PROTO_FIELD_POLY_TYPES = 11;
inline constexpr int PROTO_FIELD_POLY_NAME = 1;
inline constexpr int PROTO_FIELD_SERIALIZER_LIST = 1;
inline constexpr int PROTO_FIELD_SYMBOL_LIST = 2;
inline constexpr int PROTO_FIELD_FIELD_LIST = 3;
inline constexpr int PROTO_FIELD_SER_NAME = 1;
inline constexpr int PROTO_FIELD_SER_VERSION = 2;
inline constexpr std::size_t POLY_DEFAULT_OFFSET = 1;

float floatField(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = 0;
    for (std::size_t idx = 0; idx < FLOAT_BYTES && idx < bytes.size(); ++idx) {
        value |= static_cast<std::uint32_t>(bytes[idx]) << (BYTE_SHIFT * idx);
    }
    return std::bit_cast<float>(value);
}

/// ProtoFlattenedSerializer_t.fields_index (field 3), packed or unpacked.
void collectIndices(std::span<const std::uint8_t> msg, std::vector<std::int32_t>& out) {
    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        if (field->field != PROTO_FIELD_BIT_COUNT) {
            continue;
        }
        if (field->wire == WIRE_VARINT) {
            out.push_back(static_cast<std::int32_t>(field->varint));
        } else if (field->wire == WIRE_LEN) {
            ByteReader packed(field->bytes);
            while (auto val = packed.readVarintU64()) {
                out.push_back(static_cast<std::int32_t>(*val));
            }
        }
    }
}

} // namespace

EntField* EntityContext::makeField(std::span<const std::uint8_t> msg,
                                   const std::vector<std::string>& symbols) {
    auto owned = std::make_unique<EntField>();
    EntField* field = owned.get();
    const auto SYM = [&](std::uint64_t idx) -> std::string {
        return idx < symbols.size() ? symbols[static_cast<std::size_t>(idx)] : std::string{};
    };

    ByteReader reader(msg);
    std::vector<std::string> poly_names;
    while (auto proto_field = cyka::demo::readField(reader)) {
        switch (proto_field->field) {
        case PROTO_FIELD_VAR_TYPE:
            field->var_type = SYM(proto_field->varint);
            break;
        case PROTO_FIELD_VAR_NAME:
            field->var_name = SYM(proto_field->varint);
            break;
        case PROTO_FIELD_BIT_COUNT:
            field->bit_count = static_cast<std::int32_t>(proto_field->varint);
            break;
        case PROTO_FIELD_LOW_VALUE:
            if (proto_field->wire == WIRE32) {
                field->low_value = floatField(proto_field->bytes);
            }
            break;
        case PROTO_FIELD_HIGH_VALUE:
            if (proto_field->wire == WIRE32) {
                field->high_value = floatField(proto_field->bytes);
            }
            break;
        case PROTO_FIELD_ENCODE_FLAGS:
            field->encode_flags = static_cast<std::int32_t>(proto_field->varint);
            break;
        case PROTO_FIELD_SERIALIZER:
            field->serializer_name = SYM(proto_field->varint);
            break;
        case PROTO_FIELD_SEND_NODE:
            field->send_node = SYM(proto_field->varint);
            break;
        case PROTO_FIELD_ENCODER:
            field->encoder = SYM(proto_field->varint);
            break;
        case PROTO_FIELD_POLY_TYPES:
            if (proto_field->wire == WIRE_LEN) {
                ByteReader poly_reader(proto_field->bytes);
                while (auto poly_field = cyka::demo::readField(poly_reader)) {
                    if (poly_field->field == PROTO_FIELD_POLY_NAME &&
                        poly_field->wire == WIRE_VARINT) {
                        poly_names.push_back(SYM(poly_field->varint));
                    }
                }
            }
            break;
        default:
            break;
        }
    }
    if (field->send_node == "(root)") {
        field->send_node.clear();
    }
    if (field->var_name == "m_flSimulationTime" || field->var_name == "m_flAnimTime") {
        field->encoder = "simtime";
    }
    field->type = parseFieldType(field->var_type);
    if (!field->serializer_name.empty()) {
        field->serializer = serializerFor(field->serializer_name);
    }
    if (!poly_names.empty()) {
        field->poly_types.resize(poly_names.size() + POLY_DEFAULT_OFFSET);
        field->poly_types[0] = field->serializer;
        for (std::size_t idx = 0; idx < poly_names.size(); ++idx) {
            field->poly_types[idx + POLY_DEFAULT_OFFSET] = serializerFor(poly_names[idx]);
        }
        field->poly_serializer_id = next_poly_id++;
    }

    FieldModel model = FieldModel::SIMPLE;
    if (field->serializer != nullptr || !field->poly_types.empty()) {
        if ((field->type != nullptr &&
             (field->type->pointer || isPointerType(field->type->base))) ||
            !field->poly_types.empty()) {
            model = FieldModel::FIXED_TABLE;
        } else {
            model = FieldModel::VARIABLE_TABLE;
        }
    } else if (field->type->count > 0 && field->type->base != "char") {
        model = FieldModel::FIXED_ARRAY;
    } else if (field->type->base == "CUtlVector" || field->type->base == "CNetworkUtlVectorBase") {
        model = FieldModel::VARIABLE_ARRAY;
    }
    field->setModel(model);

    field_pool.push_back(std::move(owned));
    return field;
}

void EntityContext::loadFlattened(std::span<const std::uint8_t> msg) {
    std::vector<std::string> symbols;
    std::vector<std::span<const std::uint8_t>> field_msgs;
    std::vector<std::span<const std::uint8_t>> serializer_msgs;

    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        if (field->wire != WIRE_LEN) {
            continue;
        }
        if (field->field == PROTO_FIELD_SERIALIZER_LIST) {
            serializer_msgs.push_back(field->bytes);
        } else if (field->field == PROTO_FIELD_SYMBOL_LIST) {
            symbols.emplace_back(cyka::demo::asString(field->bytes));
        } else if (field->field == PROTO_FIELD_FIELD_LIST) {
            field_msgs.push_back(field->bytes);
        }
    }

    std::unordered_map<std::int32_t, EntField*> by_index;
    std::vector<std::int32_t> indices;
    for (const auto& ser_msg : serializer_msgs) {
        auto ser = std::make_unique<EntSerializer>();
        ByteReader ser_reader(ser_msg);
        while (auto field = cyka::demo::readField(ser_reader)) {
            if (field->field == PROTO_FIELD_SER_NAME && field->wire == WIRE_VARINT) {
                const auto IDX = static_cast<std::size_t>(field->varint);
                ser->name = IDX < symbols.size() ? symbols[IDX] : std::string{};
            } else if (field->field == PROTO_FIELD_SER_VERSION && field->wire == WIRE_VARINT) {
                ser->version = static_cast<std::int32_t>(field->varint);
            }
        }
        indices.clear();
        collectIndices(ser_msg, indices);
        for (const auto FIELD_IDX : indices) {
            if (FIELD_IDX < 0 || static_cast<std::size_t>(FIELD_IDX) >= field_msgs.size()) {
                continue;
            }
            auto iter = by_index.find(FIELD_IDX);
            if (iter == by_index.end()) {
                iter = by_index
                           .emplace(
                               FIELD_IDX,
                               makeField(field_msgs[static_cast<std::size_t>(FIELD_IDX)], symbols))
                           .first;
            }
            ser->addField(iter->second);
        }
        EntSerializer* raw = ser.get();
        serializer_pool.push_back(std::move(ser));
        serializers[raw->name] = raw;
        if (const auto CLASS_ITER = classes_by_name.find(raw->name);
            CLASS_ITER != classes_by_name.end()) {
            CLASS_ITER->second->serializer = raw;
            bindPolyCount(CLASS_ITER->second);
        }
    }
}

} // namespace cyka::demo::ent
