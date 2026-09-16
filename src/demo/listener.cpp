#include "cyka/demo/listener.hpp"

#include "cyka/demo/steam_id.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace cyka::demo {

namespace {

inline constexpr double MS_PER_SEC = 1000.0;

void mergeLoadoutItems(RawPlayer& player, std::vector<LoadoutItem> items) {
    for (auto& incoming : items) {
        if (incoming.def_index == 0 && incoming.item_id == 0) {
            continue;
        }
        LoadoutItem* existing = nullptr;
        if (incoming.item_id != 0) {
            for (auto& have : player.loadout) {
                if (have.item_id == incoming.item_id) {
                    existing = &have;
                    break;
                }
            }
        }
        if (existing == nullptr) {
            for (auto& have : player.loadout) {
                if (have.def_index == incoming.def_index &&
                    have.paint_index == incoming.paint_index &&
                    have.paint_seed == incoming.paint_seed) {
                    existing = &have;
                    break;
                }
            }
        }
        if (existing == nullptr) {
            player.loadout.push_back(std::move(incoming));
            continue;
        }
        if (incoming.item_id != 0 && existing->item_id == 0) {
            existing->item_id = incoming.item_id;
        }
        if (incoming.has_wear) {
            existing->paint_wear = incoming.paint_wear;
            existing->has_wear = true;
        }
        if (incoming.paint_index != 0) {
            existing->paint_index = incoming.paint_index;
        }
        if (incoming.paint_seed != 0) {
            existing->paint_seed = incoming.paint_seed;
        }
        if (!incoming.custom_name.empty()) {
            existing->custom_name = std::move(incoming.custom_name);
        }
        if (!incoming.item_name.empty()) {
            existing->item_name = std::move(incoming.item_name);
        }
        if (incoming.kill_eater_value >= 0) {
            existing->kill_eater_value = incoming.kill_eater_value;
        }
        if (!incoming.stickers.empty()) {
            existing->stickers = std::move(incoming.stickers);
        }
        if (!incoming.keychains.empty()) {
            existing->keychains = std::move(incoming.keychains);
        }
        if (incoming.team != 0) {
            existing->team = incoming.team;
        }
        if (incoming.slot >= 0) {
            existing->slot = incoming.slot;
        }
        if (incoming.rarity != 0) {
            existing->rarity = incoming.rarity;
        }
        if (incoming.quality != 0) {
            existing->quality = incoming.quality;
        }
        if (incoming.music_index != 0) {
            existing->music_index = incoming.music_index;
        }
    }
}

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

void CollectingListener::noteCrosshair(const SteamId& steam, const std::string& code) {
    if (steam.empty() || code.empty()) {
        return;
    }
    if (auto* player = findPlayer(steam)) {
        player->crosshair_code = code;
    }
}

void CollectingListener::applyLoadoutItems(const SteamId& steam, std::vector<LoadoutItem> items) {
    if (steam.empty() || items.empty()) {
        return;
    }
    ensurePlayer(steam, {}, 0);
    if (auto* player = findPlayer(steam)) {
        mergeLoadoutItems(*player, std::move(items));
    }
}

void CollectingListener::applyLoadoutBySlot(int player_slot, std::vector<LoadoutItem> items) {
    if (player_slot < 0 || items.empty()) {
        return;
    }
    // playerslot is the controller slot / userid low-byte — use the same
    // resolution path as game events (not a raw steam_by_userid lookup).
    const SteamId STEAM = steamForUserid(player_slot);
    if (STEAM.empty()) {
        return;
    }
    applyLoadoutItems(STEAM, std::move(items));
}

} // namespace cyka::demo
