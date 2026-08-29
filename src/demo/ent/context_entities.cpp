// PacketEntities decoding, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/entity.go (Parser.OnPacketEntities). See NOTICE.

#include "cyka/demo/ent/context.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <string_view>
#include <unordered_set>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::kWireLen;
using cyka::demo::kWireVarint;

/// Classes whose field values we retain; everything else is decode-and-drop.
bool is_tracked_class(std::string_view name) {
    static const std::unordered_set<std::string_view> kTracked{
        "CCSPlayerController", "CCSPlayerPawn", "CCSGameRulesProxy"};
    return kTracked.contains(name);
}

struct PacketHeader {
    std::span<const std::uint8_t> entity_data;
    int updated_entries{0};
    bool is_delta{false};
    std::uint32_t pvs_vis_bits{0};
};

PacketHeader read_header(std::span<const std::uint8_t> msg) {
    PacketHeader h;
    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        switch (f->field) {
        case 2:
            h.updated_entries = static_cast<int>(f->varint);
            break;
        case 3:
            h.is_delta = f->varint != 0;
            break;
        case 7:
            if (f->wire == kWireLen) {
                h.entity_data = f->bytes;
            }
            break;
        case 16:
            h.pvs_vis_bits = static_cast<std::uint32_t>(f->varint);
            break;
        default:
            break;
        }
    }
    return h;
}

} // namespace

Entity* EntityContext::find(std::int32_t index) const {
    const auto it = entities_.find(index);
    return it == entities_.end() ? nullptr : it->second.get();
}

Entity* EntityContext::find_by_handle(std::uint64_t handle) const {
    if (handle == kInvalidHandle) {
        return nullptr;
    }
    auto* e = find(static_cast<std::int32_t>(handle & kHandleIndexMask));
    if (e == nullptr || static_cast<std::uint64_t>(e->serial()) != (handle >> kMaxEdictBits)) {
        return nullptr;
    }
    return e;
}

void EntityContext::set_baseline(std::int32_t class_id, std::vector<std::uint8_t> data) {
    baselines_[class_id] = std::move(data);
}

bool EntityContext::on_packet_entities(std::span<const std::uint8_t> msg) {
    if (!ready()) {
        return true;
    }
    const PacketHeader h = read_header(msg);
    if (h.entity_data.empty() || h.updated_entries <= 0) {
        return true;
    }
    if (!h.is_delta) {
        if (full_packets_ > 0) {
            return true; // demoinfocs: only the first full frame seeds state
        }
        ++full_packets_;
    }

    BitStream r(h.entity_data);
    std::int32_t index = -1;

    for (int remaining = h.updated_entries; remaining > 0; --remaining) {
        index += static_cast<std::int32_t>(r.read_ubit_var()) + 1;
        if (r.failed() || index < 0 || index > 0x3FFF) {
            ++failures_;
            return false;
        }
        const std::uint32_t cmd = r.read_bits(2);

        if ((cmd & 0x01U) != 0) {
            auto* e = find(index);
            if (e == nullptr || !e->active()) {
                continue;
            }
            // Leaving PVS alone keeps the entity alive (demoinfocs EntityOpLeft);
            // only an explicit delete retires it.
            if ((cmd & 0x02U) != 0) {
                e->set_active(false);
                tracked_dirty_ = true;
            }
            continue;
        }

        if ((cmd & 0x02U) != 0) {
            const auto class_id = static_cast<std::int32_t>(r.read_bits(class_id_bits_));
            const auto serial = static_cast<std::int32_t>(r.read_bits(17));
            (void)r.read_var_u32(); // unused "creation tick" style field
            const auto ci = classes_by_id_.find(class_id);
            if (ci == classes_by_id_.end() || ci->second->serializer == nullptr) {
                ++failures_;
                return false;
            }
            auto owned = std::make_unique<Entity>(index, serial, ci->second);
            Entity* e = owned.get();
            e->set_tracked(is_tracked_class(ci->second->name));
            entities_[index] = std::move(owned);
            tracked_dirty_ = true;

            if (const auto bl = baselines_.find(class_id); bl != baselines_.end()) {
                BitStream br(bl->second);
                (void)e->read_fields(br, path_scratch_);
            }
            if (!e->read_fields(r, path_scratch_)) {
                ++failures_;
                return false;
            }
            continue;
        }

        if (h.pvs_vis_bits > 0 && (r.read_bits(2) & 0x01U) != 0) {
            continue;
        }
        auto* e = find(index);
        if (e == nullptr) {
            ++failures_;
            return false;
        }
        e->set_active(true);
        if (!e->read_fields(r, path_scratch_)) {
            ++failures_;
            return false;
        }
    }

    return true;
}

const std::vector<Entity*>& EntityContext::tracked() const {
    if (tracked_dirty_) {
        tracked_.clear();
        for (const auto& [idx, e] : entities_) {
            if (e->tracked()) {
                tracked_.push_back(e.get());
            }
        }
        tracked_dirty_ = false;
    }
    return tracked_;
}

} // namespace cyka::demo::ent
