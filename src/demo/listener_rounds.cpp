#include "cyka/demo/listener.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cyka::demo {
namespace {

[[nodiscard]] std::string elim_winner(const RawMatch& raw,
                                      const std::unordered_map<SteamId, std::string>& team_of,
                                      int round) {
    std::unordered_map<std::string, std::unordered_set<SteamId>> dead;
    const RawKill* last = nullptr;
    for (const auto& k : raw.kills) {
        if (k.round_number != round) {
            continue;
        }
        if (auto vt = team_of.find(k.victim_steam); vt != team_of.end()) {
            dead[vt->second].insert(k.victim_steam);
        }
        if (k.attacker_steam.empty() || k.attacker_steam == k.victim_steam) {
            continue;
        }
        // Ignore team-kills when tracking last frag.
        auto at = team_of.find(k.attacker_steam);
        auto vtm = team_of.find(k.victim_steam);
        if (at != team_of.end() && vtm != team_of.end() && at->second == vtm->second) {
            continue;
        }
        last = &k;
    }
    const auto da = dead["A"].size();
    const auto db = dead["B"].size();
    // Full wipe (or one side clearly cleared): survivors win.
    if (da >= 5 && db < da) {
        return "B";
    }
    if (db >= 5 && da < db) {
        return "A";
    }
    if (da > db && da >= 4 && db <= 3) {
        return "B";
    }
    if (db > da && db >= 4 && da <= 3) {
        return "A";
    }
    if (last == nullptr) {
        return {};
    }
    if (auto at = team_of.find(last->attacker_steam); at != team_of.end()) {
        return at->second;
    }
    if (auto vt = team_of.find(last->victim_steam); vt != team_of.end()) {
        return vt->second == "A" ? "B" : "A";
    }
    return {};
}

} // namespace

void CollectingListener::begin_round(Tick tick) {
    if (have_pending_) {
        close_round_inferred(tick);
    }
    ++round_number_;
    round_live_ = true;
    bomb_state_.clear();
    pending_ = RawRound{};
    pending_.number = round_number_;
    pending_.start_tick = freeze_start_ > 0 ? freeze_start_ : tick;
    pending_.freeze_end = tick;
    have_pending_ = true;
    freeze_start_ = 0;
}

void CollectingListener::end_round(Tick tick, int winner_team, std::string reason) {
    if (!have_pending_) {
        begin_round(tick);
    }
    round_live_ = false;
    pending_.end_tick = tick;
    if (pending_.reason.empty() && !reason.empty()) {
        pending_.reason = std::move(reason);
    }
    if (pending_.winner_letter.empty() && winner_team >= 2 && winner_team <= 3) {
        pending_.winner_letter = side_letter_[winner_team];
    }
}

void CollectingListener::close_round_inferred(Tick tick) {
    if (!have_pending_) {
        return;
    }
    if (pending_.end_tick == 0) {
        pending_.end_tick = tick;
    }
    if (pending_.winner_letter.empty()) {
        if (bomb_state_ == "defused") {
            pending_.winner_letter = side_letter_[3];
            pending_.reason = "bomb_defused";
        } else if (bomb_state_ == "exploded") {
            pending_.winner_letter = side_letter_[2];
            pending_.reason = "bomb_exploded";
        } else {
            pending_.reason = "elimination";
        }
    }
    // Drop freeze_end phantoms with no combat (keeps round count aligned with csda).
    bool any_combat = false;
    for (const auto& k : raw_.kills) {
        if (k.round_number == pending_.number) {
            any_combat = true;
            break;
        }
    }
    if (!any_combat) {
        for (const auto& d : raw_.damages) {
            if (d.round_number == pending_.number) {
                any_combat = true;
                break;
            }
        }
    }
    if (!any_combat && pending_.winner_letter.empty() && bomb_state_.empty()) {
        have_pending_ = false;
        round_live_ = false;
        --round_number_;
        return;
    }
    const int finished = pending_.number;
    raw_.rounds.push_back(pending_);
    have_pending_ = false;
    round_live_ = false;
    bomb_state_.clear();
    if (finished == 12) {
        std::swap(side_letter_[2], side_letter_[3]);
    }
}

void CollectingListener::finish() {
    if (have_pending_) {
        close_round_inferred(raw_.ticks);
    }
    for (auto& p : raw_.players) {
        if (auto it = team_of_.find(p.steam_id); it != team_of_.end()) {
            p.team_letter = it->second;
        }
    }
    raw_.score_a = raw_.score_b = 0;
    std::vector<RawRound> kept;
    kept.reserve(raw_.rounds.size());
    for (auto& r : raw_.rounds) {
        if (r.winner_letter.empty()) {
            r.winner_letter = elim_winner(raw_, team_of_, r.number);
            if (r.reason.empty() || r.reason == "unknown") {
                r.reason = "elimination";
            }
        }
        if (r.winner_letter.empty()) {
            continue; // unresolved phantom — do not inflate round count / ADR
        }
        if (r.winner_letter == "A") {
            ++raw_.score_a;
        } else if (r.winner_letter == "B") {
            ++raw_.score_b;
        }
        r.team_a_score = raw_.score_a;
        r.team_b_score = raw_.score_b;
        kept.push_back(std::move(r));
    }
    raw_.rounds = std::move(kept);
}

} // namespace cyka::demo
