// Send-table / class-info ingestion, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/parser.go. See NOTICE.

#include "cyka/demo/ent/context.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <cmath>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::WIRE_LEN;
using cyka::demo::WIRE_VARINT;

inline constexpr int PROTO_FIELD_SEND_TABLES = 1;
inline constexpr int PROTO_FIELD_CLASS_ID_BITS = 11;
inline constexpr int PROTO_FIELD_CLASS_LIST = 1;
inline constexpr int PROTO_FIELD_CLASS_ID = 1;
inline constexpr int PROTO_FIELD_CLASS_NAME = 2;
inline constexpr int PROTO_FIELD_SVC_CLASS_LIST = 2;
inline constexpr int PROTO_FIELD_SVC_CLASS_NAME = 3;

} // namespace

void bindPolyCount(EntClass* cls) {
    if (cls == nullptr || cls->serializer == nullptr) {
        return;
    }
    const int MAX_POLY = cls->serializer->maxPolyId();
    cls->poly_count = MAX_POLY >= 0 ? MAX_POLY + 1 : 0;
}

const EntSerializer* EntityContext::serializerFor(const std::string& name) const {
    const auto ITER = serializers.find(name);
    return ITER == serializers.end() ? nullptr : ITER->second;
}

void EntityContext::onSendTables(std::span<const std::uint8_t> body) {
    const auto DATA = cyka::demo::findBytesField(body, PROTO_FIELD_SEND_TABLES);
    if (DATA.empty()) {
        return;
    }
    ByteReader reader(DATA);
    const auto LEN = reader.readVarintU32();
    if (!LEN) {
        return;
    }
    if (auto payload = reader.readBytes(*LEN)) {
        loadFlattened(*payload);
    }
}

void EntityContext::onFlattenedSerializer(std::span<const std::uint8_t> msg) {
    loadFlattened(msg);
}

void EntityContext::onServerInfo(std::span<const std::uint8_t> msg) {
    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        if (field->field == PROTO_FIELD_CLASS_ID_BITS && field->wire == WIRE_VARINT &&
            field->varint > 1) {
            class_id_bits =
                static_cast<std::uint32_t>(std::log2(static_cast<double>(field->varint))) + 1;
        }
    }
}

void EntityContext::registerClass(std::int32_t class_id, std::string name) {
    auto cls = std::make_unique<EntClass>();
    cls->class_id = class_id;
    cls->name = std::move(name);
    cls->serializer = serializerFor(cls->name);
    bindPolyCount(cls.get());
    EntClass* raw = cls.get();
    class_pool.push_back(std::move(cls));
    classes_by_id[class_id] = raw;
    classes_by_name[raw->name] = raw;
}

void EntityContext::onDemoClassInfo(std::span<const std::uint8_t> body) {
    cyka::demo::forEachMessage(
        body, PROTO_FIELD_CLASS_LIST, [&](std::span<const std::uint8_t> cls_msg) {
            std::int32_t class_id = 0;
            std::string name;
            ByteReader reader(cls_msg);
            while (auto field = cyka::demo::readField(reader)) {
                if (field->field == PROTO_FIELD_CLASS_ID && field->wire == WIRE_VARINT) {
                    class_id = static_cast<std::int32_t>(field->varint);
                } else if (field->field == PROTO_FIELD_CLASS_NAME && field->wire == WIRE_LEN) {
                    name = std::string{cyka::demo::asString(field->bytes)};
                }
            }
            if (!name.empty()) {
                registerClass(class_id, std::move(name));
            }
        });
}

void EntityContext::onSvcClassInfo(std::span<const std::uint8_t> msg) {
    cyka::demo::forEachMessage(
        msg, PROTO_FIELD_SVC_CLASS_LIST, [&](std::span<const std::uint8_t> cls_msg) {
            std::int32_t class_id = 0;
            std::string name;
            ByteReader reader(cls_msg);
            while (auto field = cyka::demo::readField(reader)) {
                if (field->field == PROTO_FIELD_CLASS_ID && field->wire == WIRE_VARINT) {
                    class_id = static_cast<std::int32_t>(field->varint);
                } else if (field->field == PROTO_FIELD_SVC_CLASS_NAME && field->wire == WIRE_LEN) {
                    name = std::string{cyka::demo::asString(field->bytes)};
                }
            }
            if (!name.empty() && !classes_by_id.contains(class_id)) {
                registerClass(class_id, std::move(name));
            }
        });
}

} // namespace cyka::demo::ent
