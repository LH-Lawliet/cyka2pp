#include "cyka/demo/listener.hpp"

#include "cyka/demo/steam_id.hpp"

#include <cmath>
#include <utility>

namespace cyka::demo {

void CollectingListener::set_map(std::string map, std::string workshop) {
    raw_.map_name = std::move(map);
    raw_.workshop_id = std::move(workshop);
}

void CollectingListener::set_ticks(int ticks, double tickrate) {
    raw_.ticks = ticks;
    if (tickrate > 0) {
        raw_.tickrate = tickrate;
    }
    if (raw_.tickrate > 0) {
        raw_.duration_ms = static_cast<Millis>(std::llround(ticks / raw_.tickrate * 1000.0));
    }
}

void CollectingListener::on_userinfo(const UserInfoById& users) {
    users_ = users;
    for (const auto& [uid, u] : users) {
        if (u.xuid == 0 || u.ishltv || u.fakeplayer || !is_individual_steam64(u.xuid)) {
            continue;
        }
        ensure_player(std::to_string(u.xuid), u.name, u.user_id);
    }
}

SteamId CollectingListener::steam_for_userid(std::int32_t userid) const {
    if (userid < 0 || userid == 65535) {
        return {};
    }
    // Match demoinfocs playerByUserID32: low byte is the string-table slot.
    // Some CS2 players (e.g. reconnect ghosts) report userid 0 but still occupy
    // userinfo slot 0 with a real xuid — do not treat 0 as "no player".
    const std::int32_t masked =
        userid <= static_cast<std::int32_t>(0xffff) ? (userid & 0xff) : userid;

    auto it = users_.find(masked);
    if (it != users_.end() && is_individual_steam64(it->second.xuid) && !it->second.ishltv) {
        return std::to_string(it->second.xuid);
    }
    if (userid != 0) {
        it = users_.find(userid);
        if (it != users_.end() && is_individual_steam64(it->second.xuid)) {
            return std::to_string(it->second.xuid);
        }
        it = users_.find(masked + 1);
        if (it != users_.end() && is_individual_steam64(it->second.xuid)) {
            return std::to_string(it->second.xuid);
        }
    }
    return {};
}

std::string CollectingListener::name_for_userid(std::int32_t userid) const {
    if (userid <= 0) {
        return {};
    }
    const std::int32_t masked =
        userid <= static_cast<std::int32_t>(0xffff) ? (userid & 0xff) : userid;
    auto it = users_.find(masked);
    if (it != users_.end()) {
        return it->second.name;
    }
    it = users_.find(userid);
    return it == users_.end() ? std::string{} : it->second.name;
}

void CollectingListener::ensure_player(const SteamId& steam, const std::string& name, int userid) {
    if (steam.empty() || !is_individual_steam64(steam)) {
        return;
    }
    for (auto& p : raw_.players) {
        if (p.steam_id == steam) {
            if (looks_like_player_name(name)) {
                p.name = name;
            }
            if (userid) {
                p.user_id = userid;
            }
            return;
        }
    }
    RawPlayer p;
    p.steam_id = steam;
    p.name = looks_like_player_name(name) ? name : steam;
    p.user_id = userid;
    // Leave team unset until player_team / finish(); defaulting to "A" kept
    // inactive entity ghosts on the scoreboard.
    raw_.players.push_back(std::move(p));
}

RawPlayer* CollectingListener::find_player(const SteamId& steam) {
    if (steam.empty()) {
        return nullptr;
    }
    for (auto& p : raw_.players) {
        if (p.steam_id == steam) {
            return &p;
        }
    }
    return nullptr;
}

void CollectingListener::note_team(const SteamId& steam, int team) {
    if (steam.empty() || team < 2 || team > 3) {
        return;
    }
    if (!team_of_.contains(steam)) {
        team_of_[steam] = side_letter_[team];
    }
}

} // namespace cyka::demo
