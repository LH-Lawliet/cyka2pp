// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/entity.go
// (Entity.readFields). See NOTICE.

#include "cyka/demo/ent/entity.hpp"

namespace cyka::demo::ent {

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
        const DecodeSel sel = cls_->serializer->select(fp, 0);
        if (!sel.ok || sel.spec == nullptr) {
            return false;
        }
        EntValue v = decode_value(*sel.spec, r);
        if (sel.spec->op == DecOp::PolyBase && sel.field != nullptr) {
            sel.field->apply_poly(v.u);
        }
        if (tracked_) {
            state_[fp.key()] = std::move(v);
        }
        if (r.failed()) {
            return false;
        }
    }
    return true;
}

} // namespace cyka::demo::ent
