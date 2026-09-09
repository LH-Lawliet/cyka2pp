#include "cyka/demo/listener.hpp"

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::demo {
namespace {

inline constexpr std::size_t TEAM_WIPE_SIZE = 5;
inline constexpr std::size_t WINGMAN_WIPE_SIZE = 2;
inline constexpr std::size_t NEAR_WIPE_DEAD = 4;
inline constexpr std::size_t NEAR_WIPE_SURVIVORS = 3;
inline constexpr int COLOR_A = 1;
inline constexpr int COLOR_B = 2;
inline constexpr int COLOR_SUM = 3;
inline constexpr int SMOKE_LIFE_SECS = 20;

[[nodiscard]] constexpr std::size_t teamWipeSize(int rank_type) {
    return rank_type == RANK_WINGMAN ? WINGMAN_WIPE_SIZE : TEAM_WIPE_SIZE;
}

[[nodiscard]] std::string elimWinner(
    const RawMatch& raw, const std::unordered_map<SteamId, std::string>& team_of, int round) {
    std::unordered_map<std::string, std::unordered_set<SteamId>> dead;
    const RawKill* last = nullptr;
    for (const auto& kill : raw.kills) {
        if (kill.round_number != round) {
            continue;
        }
        if (auto victim_team = team_of.find(kill.victim_steam); victim_team != team_of.end()) {
            dead[victim_team->second].insert(kill.victim_steam);
        }
        if (kill.attacker_steam.empty() || kill.attacker_steam == kill.victim_steam) {
            continue;
        }
        auto attacker_team = team_of.find(kill.attacker_steam);
        auto victim_team = team_of.find(kill.victim_steam);
        if (attacker_team != team_of.end() && victim_team != team_of.end() &&
            attacker_team->second == victim_team->second) {
            continue;
        }
        last = &kill;
    }
    const auto DEAD_A = dead["A"].size();
    const auto DEAD_B = dead["B"].size();
    const auto WIPE = teamWipeSize(observedRankType(raw));
    if (DEAD_A >= WIPE && DEAD_B < DEAD_A) {
        return "B";
    }
    if (DEAD_B >= WIPE && DEAD_A < DEAD_B) {
        return "A";
    }
    if (WIPE >= TEAM_WIPE_SIZE && DEAD_A > DEAD_B && DEAD_A >= NEAR_WIPE_DEAD &&
        DEAD_B <= NEAR_WIPE_SURVIVORS) {
        return "B";
    }
    if (WIPE >= TEAM_WIPE_SIZE && DEAD_B > DEAD_A && DEAD_B >= NEAR_WIPE_DEAD &&
        DEAD_A <= NEAR_WIPE_SURVIVORS) {
        return "A";
    }
    if (last == nullptr) {
        return {};
    }
    if (auto attacker_team = team_of.find(last->attacker_steam); attacker_team != team_of.end()) {
        return attacker_team->second;
    }
    if (auto victim_team = team_of.find(last->victim_steam); victim_team != team_of.end()) {
        return victim_team->second == "A" ? "B" : "A";
    }
    return {};
}

} // namespace

void CollectingListener::inferTeamsFromKills() {
    std::unordered_map<SteamId, std::vector<SteamId>> enemies;
    for (const auto& kill : raw().kills) {
        if (kill.attacker_steam.empty() || kill.victim_steam.empty() ||
            kill.attacker_steam == kill.victim_steam) {
            continue;
        }
        enemies[kill.attacker_steam].push_back(kill.victim_steam);
        enemies[kill.victim_steam].push_back(kill.attacker_steam);
    }
    if (enemies.empty()) {
        return;
    }

    std::unordered_map<SteamId, int> color;
    for (const auto& [sid, letter] : team_of) {
        color[sid] = letter == "B" ? COLOR_B : COLOR_A;
    }

    auto try_paint = [&](const SteamId& start, int start_color) {
        if (color.contains(start) && color[start] != start_color) {
            return;
        }
        std::queue<SteamId> queue;
        if (!color.contains(start)) {
            color[start] = start_color;
        }
        queue.push(start);
        while (!queue.empty()) {
            const SteamId CUR = queue.front();
            queue.pop();
            const int CUR_COLOR = color[CUR];
            for (const auto& neighbor : enemies[CUR]) {
                auto iter = color.find(neighbor);
                if (iter == color.end()) {
                    color[neighbor] = COLOR_SUM - CUR_COLOR;
                    queue.push(neighbor);
                }
            }
        }
    };

    for (const auto& [sid, seed_color] : std::unordered_map<SteamId, int>(color)) {
        try_paint(sid, seed_color);
    }
    for (const auto& [sid, unused_neighbors] : enemies) {
        (void)unused_neighbors;
        if (!color.contains(sid)) {
            try_paint(sid, COLOR_A);
        }
    }

    for (const auto& [sid, team_color] : color) {
        if (!team_of.contains(sid)) {
            team_of[sid] = team_color == COLOR_B ? "B" : "A";
        }
    }
}

void CollectingListener::beginRound(Tick tick) {
    if (have_pending) {
        closeRoundInferred(tick);
    }
    ++round_number;
    round_live = true;
    bomb_state.clear();
    pending = RawRound{};
    pending.number = round_number;
    pending.start_tick = freeze_start > 0 ? freeze_start : tick;
    pending.freeze_end = tick;
    have_pending = true;
    freeze_start = 0;
}

void CollectingListener::endRound(RoundEnd end) {
    if (!have_pending) {
        beginRound(end.tick);
    }
    round_live = false;
    pending.end_tick = end.tick;
    // Authoritative sources (round_end event / GameRules) always win over inference.
    if (end.winner_team >= TEAM_T && end.winner_team <= TEAM_CT) {
        pending.winner_letter = side_letter[static_cast<std::size_t>(end.winner_team)];
    }
    if (!end.reason.empty()) {
        pending.reason = std::move(end.reason);
    }
}

void CollectingListener::closeRoundInferred(Tick tick) {
    if (!have_pending) {
        return;
    }
    if (pending.end_tick == 0) {
        pending.end_tick = tick;
    }
    if (pending.winner_letter.empty()) {
        if (bomb_state == "defused") {
            pending.winner_letter = side_letter[static_cast<std::size_t>(TEAM_CT)];
            pending.reason = "bomb_defused";
        } else if (bomb_state == "exploded") {
            pending.winner_letter = side_letter[static_cast<std::size_t>(TEAM_T)];
            pending.reason = "bomb_exploded";
        } else {
            pending.reason = "elimination";
        }
    }
    bool any_combat = false;
    for (const auto& kill : raw().kills) {
        if (kill.round_number == pending.number) {
            any_combat = true;
            break;
        }
    }
    if (!any_combat) {
        for (const auto& damage : raw().damages) {
            if (damage.round_number == pending.number) {
                any_combat = true;
                break;
            }
        }
    }
    if (!any_combat && pending.winner_letter.empty() && bomb_state.empty()) {
        have_pending = false;
        round_live = false;
        --round_number;
        return;
    }
    raw().rounds.push_back(pending);
    have_pending = false;
    round_live = false;
    bomb_state.clear();
    // Prefer m_bSwitchingTeamsAtRoundReset. Fallback: swap after N *scored*
    // rounds (not round_number — empty phantoms like world-only deaths would
    // fire half-time one round early and invert post-half winners).
    if (sides_swapped_by_rules) {
        return;
    }
    int scored = 0;
    for (const auto& round : raw().rounds) {
        if (!round.winner_letter.empty()) {
            ++scored;
        }
    }
    // Regulation half, then OT: swap at 2*half (OT start) and every OT half after.
    const int HALF = halfRoundsForRankType(observedRankType(raw()));
    constexpr int OT_HALF = 3;
    bool do_swap = scored == HALF;
    if (!do_swap && scored >= HALF * 2) {
        do_swap = (scored - HALF * 2) % OT_HALF == 0;
    }
    if (do_swap) {
        std::swap(side_letter[static_cast<std::size_t>(TEAM_T)],
                  side_letter[static_cast<std::size_t>(TEAM_CT)]);
    }
}

void CollectingListener::finish() {
    if (have_pending) {
        closeRoundInferred(raw().ticks);
    }
    inferTeamsFromKills();
    for (auto& player : raw().players) {
        if (auto iter = team_of.find(player.steam_id); iter != team_of.end()) {
            player.team_letter = iter->second;
        }
    }
    raw().score_a = raw().score_b = 0;
    std::vector<RawRound> kept;
    kept.reserve(raw().rounds.size());
    for (auto& round : raw().rounds) {
        if (round.winner_letter.empty()) {
            round.winner_letter = elimWinner(raw(), team_of, round.number);
            if (round.reason.empty() || round.reason == "unknown") {
                round.reason = "elimination";
            }
        }
        if (round.winner_letter.empty()) {
            continue;
        }
        if (round.winner_letter == "A") {
            ++raw().score_a;
        } else if (round.winner_letter == "B") {
            ++raw().score_b;
        }
        round.team_a_score = raw().score_a;
        round.team_b_score = raw().score_b;
        kept.push_back(std::move(round));
    }
    raw_match.rounds = std::move(kept);
    const double TICKRATE = raw_match.tickrate > 0 ? raw_match.tickrate : DEFAULT_TICKRATE;
    const int SMOKE_LIFE = static_cast<int>(TICKRATE * SMOKE_LIFE_SECS);
    for (auto& smoke : raw_match.smokes) {
        if (smoke.end_tick == 0) {
            smoke.end_tick = smoke.start_tick + SMOKE_LIFE;
        }
    }
}

} // namespace cyka::demo
