// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/entity.go
// (Entity.readFields). See NOTICE.

#include "cyka/demo/ent/entity.hpp"

namespace cyka::demo::ent {

void Entity::apply_poly(int id, const EntSerializer* ser) {
    if (id < 0) {
        return;
    }
    const auto i = static_cast<std::size_t>(id);
    if (i >= poly_serializers_.size()) {
        poly_serializers_.resize(i + 1, nullptr);
    }
    poly_serializers_[i] = ser;
}

bool Entity::read_fields(BitStream& r, std::vector<FieldPath>& scratch) {
    if (cls_ == nullptr || cls_->serializer == nullptr) {
        return false;
    }
    const int n = read_field_paths(r, scratch);
    if (n < 0) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const FieldPath& fp = scratch[static_cast<std::size_t>(i)];
        const DecodeSel sel = cls_->serializer->select(fp, 0, poly_serializers_);
        if (!sel.ok || sel.spec == nullptr) {
            return false;
        }
        EntValue v = decode_value(*sel.spec, r);
        if (sel.spec->op == DecOp::PolyBase && sel.field != nullptr) {
            const EntSerializer* ser = nullptr;
            if (v.b) {
                if (v.u >= sel.field->poly_types.size()) {
                    return false;
                }
                ser = sel.field->poly_types[static_cast<std::size_t>(v.u)];
            }
            apply_poly(sel.field->poly_serializer_id, ser);
            if (tracked_) {
                state_[fp.key()] = EntValue::of_bool(v.b);
            }
        } else if (tracked_) {
            state_[fp.key()] = std::move(v);
        }
        if (r.failed()) {
            return false;
        }
    }
    return true;
}

} // namespace cyka::demo::ent
