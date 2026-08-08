// Send-table / class-info ingestion, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/parser.go. See NOTICE.

#include "cyka/demo/ent/context.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <cmath>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::kWireLen;
using cyka::demo::kWireVarint;

} // namespace

const EntSerializer* EntityContext::serializer_for(const std::string& name) const {
    const auto it = serializers_.find(name);
    return it == serializers_.end() ? nullptr : it->second;
}

void EntityContext::on_send_tables(std::span<const std::uint8_t> body) {
    const auto data = cyka::demo::find_bytes_field(body, 1);
    if (data.empty()) {
        return;
    }
    ByteReader r(data);
    const auto len = r.read_varint_u32();
    if (!len) {
        return;
    }
    if (auto payload = r.read_bytes(*len)) {
        load_flattened(*payload);
    }
}

void EntityContext::on_flattened_serializer(std::span<const std::uint8_t> msg) {
    load_flattened(msg);
}

void EntityContext::on_server_info(std::span<const std::uint8_t> msg) {
    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        if (f->field == 11 && f->wire == kWireVarint && f->varint > 1) {
            class_id_bits_ =
                static_cast<std::uint32_t>(std::log2(static_cast<double>(f->varint))) + 1;
        }
    }
}

void EntityContext::register_class(std::int32_t class_id, std::string name) {
    auto cls = std::make_unique<EntClass>();
    cls->class_id = class_id;
    cls->name = std::move(name);
    cls->serializer = serializer_for(cls->name);
    EntClass* raw = cls.get();
    class_pool_.push_back(std::move(cls));
    classes_by_id_[class_id] = raw;
    classes_by_name_[raw->name] = raw;
}

void EntityContext::on_demo_class_info(std::span<const std::uint8_t> body) {
    cyka::demo::for_each_message(body, 1, [&](std::span<const std::uint8_t> c) {
        std::int32_t id = 0;
        std::string name;
        ByteReader r(c);
        while (auto f = cyka::demo::read_field(r)) {
            if (f->field == 1 && f->wire == kWireVarint) {
                id = static_cast<std::int32_t>(f->varint);
            } else if (f->field == 2 && f->wire == kWireLen) {
                name = std::string{cyka::demo::as_string(f->bytes)};
            }
        }
        if (!name.empty()) {
            register_class(id, std::move(name));
        }
    });
}

void EntityContext::on_svc_class_info(std::span<const std::uint8_t> msg) {
    cyka::demo::for_each_message(msg, 2, [&](std::span<const std::uint8_t> c) {
        std::int32_t id = 0;
        std::string name;
        ByteReader r(c);
        while (auto f = cyka::demo::read_field(r)) {
            if (f->field == 1 && f->wire == kWireVarint) {
                id = static_cast<std::int32_t>(f->varint);
            } else if (f->field == 3 && f->wire == kWireLen) {
                name = std::string{cyka::demo::as_string(f->bytes)};
            }
        }
        if (!name.empty() && !classes_by_id_.contains(id)) {
            register_class(id, std::move(name));
        }
    });
}

} // namespace cyka::demo::ent
