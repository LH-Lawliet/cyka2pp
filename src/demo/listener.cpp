#include "cyka/demo/listener.hpp"

#include "cyka/demo/steam_id.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace cyka::demo {

namespace {

inline constexpr double MS_PER_SEC = 1000.0;

} // namespace

void CollectingListener::setMap(std::string map, std::string workshop) {
    raw().map_name = std::move(map);
    raw().workshop_id = std::move(workshop);
}

void CollectingListener::setTicks(TickClock clock) {
    raw().ticks = clock.ticks;
    if (clock.tickrate > 0) {
        raw().tickrate = clock.tickrate;
    }
    if (raw().tickrate > 0) {
        raw().duration_ms =
            static_cast<Millis>(std::llround(clock.ticks / raw().tickrate * MS_PER_SEC));
    }
}

void CollectingListener::onUserinfo(const UserInfoById& users) {
    this->users = users;
    for (const auto& [_uid, user] : users) {
        if (user.xuid == 0 || user.ishltv || user.fakeplayer || !isIndividualSteam64(user.xuid)) {
            continue;
        }
        ensurePlayer(std::to_string(user.xuid), user.name, user.user_id);
        if (user.slot >= 0) {
            noteUserid(std::to_string(user.xuid), user.slot);
        }
    }
}

SteamId CollectingListener::steamForUserid(std::int32_t userid) const {
    return lookupSteamForUserid(users, steam_by_userid, userid);
}

std::string CollectingListener::nameForUserid(std::int32_t userid) const {
    if (userid < 0 || userid == INVALID_USERID) {
        return {};
    }
    const SteamId SID = steamForUserid(userid);
    if (SID.empty()) {
        return {};
    }
    for (const auto& player : raw().players) {
        if (player.steam_id == SID && looksLikePlayerName(player.name)) {
            return player.name;
        }
    }
    for (const auto& [_key, user] : users) {
        if (std::to_string(user.xuid) == SID && looksLikePlayerName(user.name)) {
            return user.name;
        }
    }
    return {};
}

void CollectingListener::ensurePlayer(const SteamId& steam, const std::string& name, int userid) {
    if (steam.empty() || !isIndividualSteam64(steam)) {
        return;
    }
    for (auto& player : raw().players) {
        if (player.steam_id == steam) {
            if (looksLikePlayerName(name)) {
                player.name = name;
            }
            if (userid != 0 && (player.user_id == 0 ||
                                (userid >= MAX_USER_SLOTS && player.user_id < MAX_USER_SLOTS))) {
                player.user_id = userid;
            }
            noteUserid(steam, userid);
            return;
        }
    }
    RawPlayer player;
    player.steam_id = steam;
    player.name = looksLikePlayerName(name) ? name : steam;
    player.user_id = userid;
    noteUserid(steam, userid);
    // Leave team unset until player_team / finish(); defaulting to "A" kept
    // inactive entity ghosts on the scoreboard.
    raw().players.push_back(std::move(player));
}

void CollectingListener::noteUserid(const SteamId& steam, int userid) {
    if (steam.empty() || userid == 0 || userid == INVALID_USERID || !isIndividualSteam64(steam)) {
        return;
    }
    steam_by_userid.try_emplace(userid, steam);
}

RawPlayer* CollectingListener::findPlayer(const SteamId& steam) {
    if (steam.empty()) {
        return nullptr;
    }
    for (auto& player : raw().players) {
        if (player.steam_id == steam) {
            return &player;
        }
    }
    return nullptr;
}

void CollectingListener::noteTeam(const SteamId& steam, int team) {
    if (steam.empty() || team < TEAM_T || team > TEAM_CT) {
        return;
    }
    if (!team_of.contains(steam)) {
        team_of[steam] = side_letter[team];
    }
}

void CollectingListener::noteMvpCount(const SteamId& steam, int mvp_count) {
    if (steam.empty() || mvp_count < 0) {
        return;
    }
    if (auto* player = findPlayer(steam)) {
        player->mvp_count = std::max(player->mvp_count, mvp_count);
    }
}

void CollectingListener::noteRank(const PlayerRank& rank) {
    if (rank.steam.empty()) {
        return;
    }
    auto* player = findPlayer(rank.steam);
    if (player == nullptr) {
        return;
    }
    // Prefer a known mode once seen; demos usually keep a constant RankType.
    if (rank.rank_type > 0) {
        player->rank_type = rank.rank_type;
    }
    // Ranking can start at 0 (unranked) then populate; keep the highest.
    player->ranking = std::max(player->ranking, rank.ranking);
    player->competitive_wins = std::max(player->competitive_wins, rank.competitive_wins);
}

} // namespace cyka::demo
