// PacketEntities decoding, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/entity.go (Parser.OnPacketEntities). See NOTICE.

#include "cyka/demo/ent/context.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <string_view>
#include <unordered_set>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::WIRE_LEN;

inline constexpr int PROTO_FIELD_UPDATED = 2;
inline constexpr int PROTO_FIELD_IS_DELTA = 3;
inline constexpr int PROTO_FIELD_ENTITY_DATA = 7;
inline constexpr int PROTO_FIELD_PVS_BITS = 16;
inline constexpr int MAX_ENTITY_INDEX = 0x3FFF;
inline constexpr unsigned CMD_BITS = 2U;
inline constexpr std::uint32_t CMD_LEAVE_BIT = 0x01U;
inline constexpr std::uint32_t CMD_DELETE_BIT = 0x02U;
inline constexpr unsigned ENTITY_SERIAL_BITS = 17U;
inline constexpr unsigned PVS_CMD_BITS = 2U;
inline constexpr std::uint32_t PVS_SKIP_BIT = 0x01U;

/// Classes whose field values we retain; everything else is decode-and-drop.
bool isTrackedClass(std::string_view name) {
    static const std::unordered_set<std::string_view> TRACKED{
        "CCSPlayerController", "CCSPlayerPawn", "CCSGameRulesProxy"};
    return TRACKED.contains(name);
}

struct PacketHeader {
    std::span<const std::uint8_t> entity_data;
    int updated_entries{0};
    bool is_delta{false};
    std::uint32_t pvs_vis_bits{0};
};

PacketHeader readHeader(std::span<const std::uint8_t> msg) {
    PacketHeader header;
    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        switch (field->field) {
        case PROTO_FIELD_UPDATED:
            header.updated_entries = static_cast<int>(field->varint);
            break;
        case PROTO_FIELD_IS_DELTA:
            header.is_delta = field->varint != 0;
            break;
        case PROTO_FIELD_ENTITY_DATA:
            if (field->wire == WIRE_LEN) {
                header.entity_data = field->bytes;
            }
            break;
        case PROTO_FIELD_PVS_BITS:
            header.pvs_vis_bits = static_cast<std::uint32_t>(field->varint);
            break;
        default:
            break;
        }
    }
    return header;
}

} // namespace

Entity* EntityContext::find(std::int32_t index) const {
    const auto ITER = entities.find(index);
    return ITER == entities.end() ? nullptr : ITER->second.get();
}

Entity* EntityContext::findByHandle(std::uint64_t handle) const {
    if (handle == INVALID_HANDLE) {
        return nullptr;
    }
    auto* ent = find(static_cast<std::int32_t>(handle & HANDLE_INDEX_MASK));
    if (ent == nullptr || static_cast<std::uint64_t>(ent->serial()) != (handle >> MAX_EDICT_BITS)) {
        return nullptr;
    }
    return ent;
}

void EntityContext::setBaseline(std::int32_t class_id, std::vector<std::uint8_t> data) {
    baselines[class_id] = std::move(data);
}

bool EntityContext::onPacketEntities(std::span<const std::uint8_t> msg) {
    if (!ready()) {
        return true;
    }
    const PacketHeader HEADER = readHeader(msg);
    if (HEADER.entity_data.empty() || HEADER.updated_entries <= 0) {
        return true;
    }
    if (!HEADER.is_delta) {
        if (full_packets > 0) {
            return true; // demoinfocs: only the first full frame seeds state
        }
        ++full_packets;
    }

    BitStream reader(HEADER.entity_data);
    std::int32_t index = -1;

    for (int remaining = HEADER.updated_entries; remaining > 0; --remaining) {
        index += static_cast<std::int32_t>(reader.readUbitVar()) + 1;
        if (reader.failed() || index < 0 || index > MAX_ENTITY_INDEX) {
            ++decode_failures;
            return false;
        }
        const std::uint32_t CMD = reader.readBits(CMD_BITS);

        if ((CMD & CMD_LEAVE_BIT) != 0) {
            auto* ent = find(index);
            if (ent == nullptr || !ent->active()) {
                continue;
            }
            if ((CMD & CMD_DELETE_BIT) != 0) {
                ent->setActive(false);
                tracked_dirty = true;
            }
            continue;
        }

        if ((CMD & CMD_DELETE_BIT) != 0) {
            const auto CLASS_ID = static_cast<std::int32_t>(reader.readBits(class_id_bits));
            const auto SERIAL = static_cast<std::int32_t>(reader.readBits(ENTITY_SERIAL_BITS));
            (void)reader.readVarU32();
            const auto CLASS_ITER = classes_by_id.find(CLASS_ID);
            if (CLASS_ITER == classes_by_id.end() || CLASS_ITER->second->serializer == nullptr) {
                ++decode_failures;
                return false;
            }
            auto owned = std::make_unique<Entity>(
                EntitySpawn{.index = index, .serial = SERIAL, .cls = CLASS_ITER->second});
            Entity* ent = owned.get();
            ent->setTracked(isTrackedClass(CLASS_ITER->second->name));
            entities[index] = std::move(owned);
            tracked_dirty = true;

            if (const auto BASELINE = baselines.find(CLASS_ID); BASELINE != baselines.end()) {
                BitStream baseline_reader(BASELINE->second);
                (void)ent->readFields(baseline_reader, path_scratch);
            }
            if (!ent->readFields(reader, path_scratch)) {
                ++decode_failures;
                return false;
            }
            continue;
        }

        if (HEADER.pvs_vis_bits > 0 && (reader.readBits(PVS_CMD_BITS) & PVS_SKIP_BIT) != 0) {
            continue;
        }
        auto* ent = find(index);
        if (ent == nullptr) {
            ++decode_failures;
            return false;
        }
        ent->setActive(true);
        if (!ent->readFields(reader, path_scratch)) {
            ++decode_failures;
            return false;
        }
    }

    return true;
}

const std::vector<Entity*>& EntityContext::tracked() const {
    if (tracked_dirty) {
        tracked_ents.clear();
        for (const auto& [idx, ent_ptr] : entities) {
            if (ent_ptr->tracked()) {
                tracked_ents.push_back(ent_ptr.get());
            }
        }
        tracked_dirty = false;
    }
    return tracked_ents;
}

} // namespace cyka::demo::ent
