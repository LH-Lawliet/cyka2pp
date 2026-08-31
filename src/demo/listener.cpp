#include "cyka/demo/listener.hpp"

#include "cyka/demo/steam_id.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cyka::demo {

namespace {

inline constexpr double MS_PER_SEC = 1000.0;
inline constexpr std::int32_t INVALID_USERID = 65535;
inline constexpr std::uint32_t USERID_BYTE_MASK = 0xffU;
inline constexpr std::int32_t USERID_SHORT_MAX = 0xffff;

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
    for (const auto& [uid, user] : users) {
        if (user.xuid == 0 || user.ishltv || user.fakeplayer || !isIndividualSteam64(user.xuid)) {
            continue;
        }
        ensurePlayer(std::to_string(user.xuid), user.name, user.user_id);
    }
}

SteamId CollectingListener::steamForUserid(std::int32_t userid) const {
    if (userid < 0 || userid == INVALID_USERID) {
        return {};
    }
    // Match demoinfocs playerByUserID32: low byte is the string-table slot.
    // Some CS2 players (e.g. reconnect ghosts) report userid 0 but still occupy
    // userinfo slot 0 with a real xuid — do not treat 0 as "no player".
    const std::int32_t MASKED =
        userid <= USERID_SHORT_MAX
            ? static_cast<std::int32_t>(static_cast<std::uint32_t>(userid) & USERID_BYTE_MASK)
            : userid;

    auto iter = users.find(MASKED);
    if (iter != users.end() && isIndividualSteam64(iter->second.xuid) && !iter->second.ishltv) {
        return std::to_string(iter->second.xuid);
    }
    if (userid != 0) {
        iter = users.find(userid);
        if (iter != users.end() && isIndividualSteam64(iter->second.xuid)) {
            return std::to_string(iter->second.xuid);
        }
        iter = users.find(MASKED + 1);
        if (iter != users.end() && isIndividualSteam64(iter->second.xuid)) {
            return std::to_string(iter->second.xuid);
        }
    }
    return {};
}

std::string CollectingListener::nameForUserid(std::int32_t userid) const {
    if (userid <= 0) {
        return {};
    }
    const std::int32_t MASKED =
        userid <= USERID_SHORT_MAX
            ? static_cast<std::int32_t>(static_cast<std::uint32_t>(userid) & USERID_BYTE_MASK)
            : userid;
    auto iter = users.find(MASKED);
    if (iter != users.end()) {
        return iter->second.name;
    }
    iter = users.find(userid);
    return iter == users.end() ? std::string{} : iter->second.name;
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
            if (userid != 0) {
                player.user_id = userid;
            }
            return;
        }
    }
    RawPlayer player;
    player.steam_id = steam;
    player.name = looksLikePlayerName(name) ? name : steam;
    player.user_id = userid;
    // Leave team unset until player_team / finish(); defaulting to "A" kept
    // inactive entity ghosts on the scoreboard.
    raw().players.push_back(std::move(player));
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
