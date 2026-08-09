#include "cyka/demo/ent/pose_sampler.hpp"

#include "cyka/demo/steam_id.hpp"

#include <string>

namespace cyka::demo::ent {
namespace {

const std::string kCellX = "CBodyComponent.m_cellX";
const std::string kCellY = "CBodyComponent.m_cellY";
const std::string kCellZ = "CBodyComponent.m_cellZ";
const std::string kVecX = "CBodyComponent.m_vecX";
const std::string kVecY = "CBodyComponent.m_vecY";
const std::string kVecZ = "CBodyComponent.m_vecZ";

const std::string kSteamId = "m_steamID";
const std::string kPlayerName = "m_iszPlayerName";
const std::string kTeamNum = "m_iTeamNum";
const std::string kPlayerPawn = "m_hPlayerPawn";
const std::string kConnected = "m_iConnected";
const std::string kMvps = "m_iMVPs";
const std::string kRankType = "m_iCompetitiveRankType";
const std::string kRanking = "m_iCompetitiveRanking";
const std::string kCompWins = "m_iCompetitiveWins";

const std::string kEyeAngles = "m_angEyeAngles";
const std::string kHealth = "m_iHealth";
const std::string kIsScoped = "m_bIsScoped";
const std::string kLifeState = "m_lifeState";
const std::string kGroundEntity = "m_hGroundEntity";

/// Source 2 splits world coordinates into a 9-bit cell index plus an offset.
double coord_from_cell(std::uint64_t cell, float offset) {
    constexpr int kCellBits = 9;
    constexpr double kMaxCoordInt = 16384.0;
    return (static_cast<double>(cell) * static_cast<double>(1 << kCellBits) - kMaxCoordInt) +
           static_cast<double>(offset);
}

bool is_controller(const Entity& e) {
    return e.cls() != nullptr && e.cls()->name == "CCSPlayerController";
}

bool read_position(const Entity& pawn, double& x, double& y, double& z) {
    const auto* cx = pawn.prop(kCellX);
    const auto* cy = pawn.prop(kCellY);
    const auto* cz = pawn.prop(kCellZ);
    const auto* ox = pawn.prop(kVecX);
    const auto* oy = pawn.prop(kVecY);
    const auto* oz = pawn.prop(kVecZ);
    if (cx == nullptr || cy == nullptr || cz == nullptr || ox == nullptr || oy == nullptr ||
        oz == nullptr) {
        return false;
    }
    x = coord_from_cell(cx->as_u64(), ox->as_f32());
    y = coord_from_cell(cy->as_u64(), oy->as_f32());
    z = coord_from_cell(cz->as_u64(), oz->as_f32());
    return true;
}

bool fill_pose(const EntityContext& ctx, const Entity& controller, PoseSample& s) {
    const auto* steam = controller.prop(kSteamId);
    if (steam == nullptr || !is_individual_steam64(steam->as_u64())) {
        return false;
    }
    const auto handle = controller.prop_u64(kPlayerPawn);
    if (!handle) {
        return false;
    }
    const Entity* pawn = ctx.find_by_handle(*handle);
    if (pawn == nullptr) {
        return false;
    }
    s.steam_id = steam->as_u64();
    if (const auto* t = controller.prop(kTeamNum); t != nullptr) {
        s.team = static_cast<int>(t->as_u64());
    }
    if (s.team != 2 && s.team != 3) {
        return false;
    }
    if (const auto* hp = pawn->prop(kHealth); hp != nullptr) {
        s.health = static_cast<int>(hp->as_i64());
    }
    const auto life = pawn->prop_u64(kLifeState);
    const bool alive = s.health > 0 || (life && *life == 0);
    if (!alive) {
        return false;
    }
    if (!read_position(*pawn, s.x, s.y, s.z)) {
        return false;
    }
    if (const auto* ang = pawn->prop(kEyeAngles); ang != nullptr && ang->kind == ValKind::Vec3) {
        s.pitch = ang->v3[0];
        s.yaw = ang->v3[1];
    }
    if (const auto* sc = pawn->prop(kIsScoped); sc != nullptr) {
        s.scoped = sc->as_bool();
    }
    if (const auto ge = pawn->prop_u64(kGroundEntity); ge) {
        s.airborne = *ge == kInvalidHandle;
    }
    return true;
}

} // namespace

void PoseSampler::collect_players(const EntityContext& ctx, std::vector<PlayerIdent>& out) const {
    out.clear();
    for (const Entity* e : ctx.tracked()) {
        if (!is_controller(*e)) {
            continue;
        }
        const auto* steam = e->prop(kSteamId);
        if (steam == nullptr || !is_individual_steam64(steam->as_u64())) {
            continue;
        }
        PlayerIdent id;
        id.steam_id = steam->as_u64();
        if (const auto* n = e->prop(kPlayerName); n != nullptr && n->kind == ValKind::Str &&
                                                  looks_like_player_name(n->s)) {
            id.name = n->s;
        }
        if (const auto* t = e->prop(kTeamNum); t != nullptr) {
            id.team = static_cast<int>(t->as_u64());
        }
        if (const auto* c = e->prop(kConnected); c != nullptr) {
            id.connected = c->as_u64() == 0; // PlayerConnectedState::Connected
        }
        if (const auto* m = e->prop(kMvps); m != nullptr) {
            id.mvp_count = static_cast<int>(m->as_i64());
        }
        if (const auto* rt = e->prop(kRankType); rt != nullptr) {
            id.rank_type = static_cast<int>(rt->as_i64());
        }
        if (const auto* rk = e->prop(kRanking); rk != nullptr) {
            id.ranking = static_cast<int>(rk->as_i64());
        }
        if (const auto* rw = e->prop(kCompWins); rw != nullptr) {
            id.competitive_wins = static_cast<int>(rw->as_i64());
        }
        // Spectators / disconnected controllers still appear in PacketEntities.
        if (!id.connected || (id.team != 2 && id.team != 3)) {
            continue;
        }
        out.push_back(std::move(id));
    }
}

void PoseSampler::collect_poses(const EntityContext& ctx, std::vector<PoseSample>& out) const {
    out.clear();
    for (const Entity* e : ctx.tracked()) {
        if (!e->active() || !is_controller(*e)) {
            continue;
        }
        PoseSample s;
        if (fill_pose(ctx, *e, s)) {
            out.push_back(s);
        }
    }
}

bool PoseSampler::pose_for(const EntityContext& ctx, std::uint64_t steam_id, PoseSample& out) const {
    for (const Entity* e : ctx.tracked()) {
        if (!e->active() || !is_controller(*e)) {
            continue;
        }
        const auto* steam = e->prop(kSteamId);
        if (steam == nullptr || steam->as_u64() != steam_id) {
            continue;
        }
        return fill_pose(ctx, *e, out);
    }
    return false;
}

} // namespace cyka::demo::ent
