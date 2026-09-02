#include "cyka/demo/ent/pose_sampler.hpp"

#include "cyka/demo/steam_id.hpp"

#include <algorithm>
#include <string>

namespace cyka::demo::ent {
namespace {

inline constexpr int TEAM_T = 2;
inline constexpr int TEAM_CT = 3;
inline constexpr int MAX_PLAYER_SLOT = 63;
inline constexpr int CELL_BITS = 9;
inline constexpr double MAX_COORD_INT = 16384.0;
inline constexpr std::size_t PITCH_IDX = 0;
inline constexpr std::size_t YAW_IDX = 1;
inline constexpr float DUCKING_MIN = 0.85F;
inline constexpr std::uint64_t FL_DUCKING = 1ULL << 1U;

const std::string CELL_X = "CBodyComponent.m_cellX";
const std::string CELL_Y = "CBodyComponent.m_cellY";
const std::string CELL_Z = "CBodyComponent.m_cellZ";
const std::string VEC_X = "CBodyComponent.m_vecX";
const std::string VEC_Y = "CBodyComponent.m_vecY";
const std::string VEC_Z = "CBodyComponent.m_vecZ";

const std::string STEAM_ID = "m_steamID";
const std::string PLAYER_NAME = "m_iszPlayerName";
const std::string USER_ID = "m_iUserID";
const std::string TEAM_NUM = "m_iTeamNum";
const std::string PLAYER_PAWN = "m_hPlayerPawn";
const std::string MVPS = "m_iMVPs";
const std::string RANK_TYPE = "m_iCompetitiveRankType";
const std::string RANKING = "m_iCompetitiveRanking";
const std::string COMP_WINS = "m_iCompetitiveWins";

const std::string EYE_ANGLES = "m_angEyeAngles";
const std::string HEALTH = "m_iHealth";
const std::string IS_SCOPED = "m_bIsScoped";
const std::string LIFE_STATE = "m_lifeState";
const std::string GROUND_ENTITY = "m_hGroundEntity";
const std::string DUCK_AMOUNT = "m_pMovementServices.m_flDuckAmount";
const std::string DUCKED = "m_pMovementServices.m_bDucked";
const std::string DUCKING = "m_pMovementServices.m_bDucking";
const std::string FLAGS = "m_fFlags";

/// Source 2 splits world coordinates into a 9-bit cell index plus an offset.
double coordFromCell(std::uint64_t cell, float offset) {
    return ((static_cast<double>(cell) *
             static_cast<double>(1U << static_cast<unsigned>(CELL_BITS))) -
            MAX_COORD_INT) +
           static_cast<double>(offset);
}

bool isController(const Entity& ent) {
    return ent.cls() != nullptr && ent.cls()->name == "CCSPlayerController";
}

struct ReadPosition {
    const Entity* pawn;
    double* pos_x;
    double* pos_y;
    double* pos_z;
};

bool readPosition(const ReadPosition& query) {
    const auto* cell_x = query.pawn->prop(CELL_X);
    const auto* cell_y = query.pawn->prop(CELL_Y);
    const auto* cell_z = query.pawn->prop(CELL_Z);
    const auto* vec_x = query.pawn->prop(VEC_X);
    const auto* vec_y = query.pawn->prop(VEC_Y);
    const auto* vec_z = query.pawn->prop(VEC_Z);
    if (cell_x == nullptr || cell_y == nullptr || cell_z == nullptr || vec_x == nullptr ||
        vec_y == nullptr || vec_z == nullptr) {
        return false;
    }
    *query.pos_x = coordFromCell(cell_x->asU64(), vec_x->asF32());
    *query.pos_y = coordFromCell(cell_y->asU64(), vec_y->asF32());
    *query.pos_z = coordFromCell(cell_z->asU64(), vec_z->asF32());
    return true;
}

bool fillPose(const EntityContext& ctx, const Entity& controller, PoseSample& sample) {
    const auto* steam = controller.prop(STEAM_ID);
    if (steam == nullptr || !isIndividualSteam64(steam->asU64())) {
        return false;
    }
    const auto HANDLE = controller.propU64(PLAYER_PAWN);
    if (!HANDLE) {
        return false;
    }
    const Entity* pawn = ctx.findByHandle(*HANDLE);
    if (pawn == nullptr) {
        return false;
    }
    sample.steam_id = steam->asU64();
    if (const auto* team = controller.prop(TEAM_NUM); team != nullptr) {
        sample.team_num = static_cast<int>(team->asU64());
    }
    if (sample.team_num != TEAM_T && sample.team_num != TEAM_CT) {
        return false;
    }
    if (const auto* health = pawn->prop(HEALTH); health != nullptr) {
        sample.health = static_cast<int>(health->asI64());
    }
    const auto LIFE = pawn->propU64(LIFE_STATE);
    const bool ALIVE = sample.health > 0 || (LIFE && *LIFE == 0);
    if (!ALIVE) {
        return false;
    }
    if (!readPosition({.pawn = pawn,
                       .pos_x = &sample.pos_x,
                       .pos_y = &sample.pos_y,
                       .pos_z = &sample.pos_z})) {
        return false;
    }
    if (const auto* angles = pawn->prop(EYE_ANGLES);
        angles != nullptr && angles->kind == ValKind::VEC3) {
        sample.pitch = angles->v3[PITCH_IDX];
        sample.yaw = angles->v3[YAW_IDX];
    }
    if (const auto* scoped = pawn->prop(IS_SCOPED); scoped != nullptr) {
        sample.scoped = scoped->asBool();
    }
    if (const auto GROUND = pawn->propU64(GROUND_ENTITY); GROUND) {
        sample.airborne = *GROUND == INVALID_HANDLE;
    }

    float duck = 0.F;
    if (const auto* amount = pawn->prop(DUCK_AMOUNT); amount != nullptr) {
        duck = std::clamp(amount->asF32(), 0.F, 1.F);
    }
    if (const auto* ducked = pawn->prop(DUCKED); ducked != nullptr && ducked->asBool()) {
        duck = std::max(duck, 1.F);
    } else if (const auto* ducking = pawn->prop(DUCKING); ducking != nullptr && ducking->asBool()) {
        duck = std::max(duck, DUCKING_MIN);
    }
    if (const auto* flags = pawn->prop(FLAGS); flags != nullptr) {
        if ((flags->asU64() & FL_DUCKING) != 0) {
            duck = std::max(duck, 1.F);
        }
    }
    sample.duck_amount = duck;
    return true;
}

} // namespace

void PoseSampler::collectPlayers(const EntityContext& ctx, std::vector<PlayerIdent>& out) {
    out.clear();
    for (const Entity* ent : ctx.tracked()) {
        if (!isController(*ent)) {
            continue;
        }
        const auto* steam = ent->prop(STEAM_ID);
        if (steam == nullptr || !isIndividualSteam64(steam->asU64())) {
            continue;
        }
        PlayerIdent ident;
        ident.steam_id = steam->asU64();
        if (const auto* name = ent->prop(PLAYER_NAME);
            name != nullptr && name->kind == ValKind::STR && looksLikePlayerName(name->s)) {
            ident.name = name->s;
        }
        if (const auto* team = ent->prop(TEAM_NUM); team != nullptr) {
            ident.team_num = static_cast<int>(team->asU64());
        }
        if (const auto* user_id = ent->prop(USER_ID); user_id != nullptr) {
            ident.user_id = normalizeUserid(user_id->asI64());
            if (ident.user_id == INVALID_USERID) {
                ident.user_id = 0;
            }
        }
        if (ent->index() >= 1 && ent->index() <= MAX_PLAYER_SLOT + 1) {
            ident.slot = ent->index() - 1;
        }
        if (const auto* mvps = ent->prop(MVPS); mvps != nullptr) {
            ident.mvp_count = static_cast<int>(mvps->asI64());
        }
        if (const auto* rank_type = ent->prop(RANK_TYPE); rank_type != nullptr) {
            ident.rank_type = static_cast<int>(rank_type->asI64());
        }
        if (const auto* ranking = ent->prop(RANKING); ranking != nullptr) {
            ident.ranking = static_cast<int>(ranking->asI64());
        }
        if (const auto* comp_wins = ent->prop(COMP_WINS); comp_wins != nullptr) {
            ident.competitive_wins = static_cast<int>(comp_wins->asI64());
        }
        if (ident.team_num != TEAM_T && ident.team_num != TEAM_CT) {
            continue;
        }
        out.push_back(std::move(ident));
    }
}

void PoseSampler::collectPoses(const EntityContext& ctx, std::vector<PoseSample>& out) {
    out.clear();
    for (const Entity* ent : ctx.tracked()) {
        if (!ent->active() || !isController(*ent)) {
            continue;
        }
        PoseSample sample;
        if (fillPose(ctx, *ent, sample)) {
            out.push_back(sample);
        }
    }
}

bool PoseSampler::poseFor(const EntityContext& ctx, std::uint64_t steam_id, PoseSample& out) {
    for (const Entity* ent : ctx.tracked()) {
        if (!ent->active() || !isController(*ent)) {
            continue;
        }
        const auto* steam = ent->prop(STEAM_ID);
        if (steam == nullptr || steam->asU64() != steam_id) {
            continue;
        }
        return fillPose(ctx, *ent, out);
    }
    return false;
}

} // namespace cyka::demo::ent
