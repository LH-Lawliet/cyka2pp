// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/entity.go
// (Entity.readFields). See NOTICE.

#include "cyka/demo/ent/entity.hpp"

namespace cyka::demo::ent {

void Entity::applyPoly(int poly_id, const EntSerializer* ser) {
    if (poly_id < 0) {
        return;
    }
    const auto IDX = static_cast<std::size_t>(poly_id);
    if (IDX >= poly_serializers.size()) {
        poly_serializers.resize(IDX + 1, nullptr);
    }
    poly_serializers[IDX] = ser;
}

bool Entity::readFields(BitStream& reader, std::vector<FieldPath>& scratch) {
    if (ent_class == nullptr || ent_class->serializer == nullptr) {
        return false;
    }
    const int COUNT = readFieldPaths(reader, scratch);
    if (COUNT < 0) {
        return false;
    }
    for (int idx = 0; idx < COUNT; ++idx) {
        const FieldPath& fpath = scratch[static_cast<std::size_t>(idx)];
        const DecodeSel SEL = ent_class->serializer->select(fpath, 0, poly_serializers);
        if (!SEL.ok || SEL.spec == nullptr) {
            return false;
        }
        EntValue value = decodeValue(*SEL.spec, reader);
        if (SEL.spec->op == DecOp::POLY_BASE && SEL.field != nullptr) {
            const EntSerializer* ser = nullptr;
            if (value.b) {
                if (value.u >= SEL.field->poly_types.size()) {
                    return false;
                }
                ser = SEL.field->poly_types[static_cast<std::size_t>(value.u)];
            }
            applyPoly(SEL.field->poly_serializer_id, ser);
            if (is_tracked) {
                prop_state[fpath.key()] = EntValue::ofBool(value.b);
            }
        } else if (is_tracked) {
            prop_state[fpath.key()] = std::move(value);
        }
        if (reader.failed()) {
            return false;
        }
    }
    return true;
}

} // namespace cyka::demo::ent
