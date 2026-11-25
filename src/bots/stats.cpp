#include "bots/stats.hpp"

#include <limits>
#include <set>
#include <stdexcept>

namespace hl {

const std::vector<StatDef>& statDefs() {
  static const std::vector<StatDef> defs = {
      {"vpip", "VPIP", 0.26, 12, &PlayerStats::hands, &PlayerStats::vpip, "Voluntarily put money in the pot preflop"},
      {"pfr", "PFR", 0.18, 12, &PlayerStats::hands, &PlayerStats::pfr, "Preflop raise"},
      {"threeBet", "3-Bet", 0.07, 15, &PlayerStats::threeBetOpp, &PlayerStats::threeBet, "Re-raise vs a single raise"},
      {"foldTo3bet", "Fold to 3-Bet", 0.55, 8, &PlayerStats::foldTo3betOpp, &PlayerStats::foldTo3bet, "Fold after opening and facing a 3-bet"},
      {"steal", "Steal", 0.35, 10, &PlayerStats::stealOpp, &PlayerStats::steal, "Open-raise from CO/BTN/SB when folded to"},
      {"cbet", "C-Bet", 0.6, 8, &PlayerStats::cbetOpp, &PlayerStats::cbet, "Bet the flop as preflop aggressor"},
      {"foldToCbet", "Fold to C-Bet", 0.45, 8, &PlayerStats::foldToCbetOpp, &PlayerStats::foldToCbet, "Fold facing a flop continuation bet"},
      {"foldToBet", "Fold vs Bet", 0.42, 12, &PlayerStats::facingBet, &PlayerStats::foldVsBet, "Fold when facing any postflop bet"},
      {"aggFreq", "Agg %", 0.38, 15, &PlayerStats::postActs, &PlayerStats::aggActs, "Share of postflop actions that are bets/raises"},
      {"wtsd", "WTSD", 0.28, 10, &PlayerStats::sawFlop, &PlayerStats::wtsd, "Went to showdown after seeing the flop"},
      {"wsd", "W$SD", 0.5, 8, &PlayerStats::wtsd, &PlayerStats::wsd, "Won money at showdown"},
  };
  return defs;
}

const StatDef& statDef(const std::string& key) {
  for (auto& d : statDefs())
    if (key == d.key) return d;
  throw std::invalid_argument("unknown stat " + key);
}

const PlayerStats& StatsBook::peek(const std::string& id) const {
  static const PlayerStats empty{};
  auto it = players.find(id);
  return it == players.end() ? empty : it->second;
}

double StatsBook::rate(const std::string& id, const std::string& stat) const {
  const StatDef& d = statDef(stat);
  const PlayerStats& s = peek(id);
  return (s.*d.hit + d.prior * d.strength) / (s.*d.opp + d.strength);
}

RawStat StatsBook::raw(const std::string& id, const std::string& stat) const {
  const StatDef& d = statDef(stat);
  const PlayerStats& s = peek(id);
  RawStat r;
  r.n = s.*d.opp;
  r.k = s.*d.hit;
  if (r.n > 0) r.pct = static_cast<double>(r.k) / r.n;
  return r;
}

std::optional<double> StatsBook::af(const std::string& id) const {
  const PlayerStats& s = peek(id);
  if (s.callActs > 0) return static_cast<double>(s.aggActs) / s.callActs;
  if (s.aggActs > 0) return std::numeric_limits<double>::infinity();
  return std::nullopt;
}

void StatsBook::recordHand(const Hand& h) {
  struct Flags {
    bool vpip = false, pfr = false, threeBetOpp = false, foldTo3betOpp = false, stealOpp = false, cbetOpp = false, foldToCbetOpp = false;
  };
  std::map<std::string, Flags> flags;
  for (int seat : h.order) get(h.p(seat).id).hands++;
  int raises = 0;
  std::string opener, pfa;
  bool anyVoluntary = false, flopBetSeen = false, cbetActive = false;
  for (const LogEntry& e : h.log) {
    if (e.blind || e.type == Act::Uncalled) continue;
    PlayerStats& s = get(e.id);
    Flags& f = flags[e.id];
    if (e.street == Street::Preflop) {
      const std::string& pos = h.p(e.seat).position;
      if (raises == 0 && !anyVoluntary && !f.stealOpp && (pos == "CO" || pos == "BTN" || pos == "SB")) {
        f.stealOpp = true;
        s.stealOpp++;
        if (e.type == Act::Raise) s.steal++;
      }
      if (raises == 1 && e.id != opener && !f.threeBetOpp) {
        f.threeBetOpp = true;
        s.threeBetOpp++;
        if (e.type == Act::Raise) s.threeBet++;
      }
      if (raises == 2 && e.id == opener && !f.foldTo3betOpp) {
        f.foldTo3betOpp = true;
        s.foldTo3betOpp++;
        if (e.type == Act::Fold) s.foldTo3bet++;
      }
      if ((e.type == Act::Call || e.type == Act::Raise || e.type == Act::Bet) && !f.vpip) {
        f.vpip = true;
        s.vpip++;
      }
      if (e.type == Act::Raise && !f.pfr) {
        f.pfr = true;
        s.pfr++;
      }
      if (e.type == Act::Call || e.type == Act::Raise) anyVoluntary = true;
      if (e.type == Act::Raise) {
        ++raises;
        if (raises == 1) opener = e.id;
        pfa = e.id;
      }
      continue;
    }
    if (e.street == Street::Flop) {
      if (!flopBetSeen && e.id == pfa && !f.cbetOpp) {
        f.cbetOpp = true;
        s.cbetOpp++;
        if (e.type == Act::Bet) {
          s.cbet++;
          cbetActive = true;
        }
      } else if (cbetActive && e.id != pfa && !f.foldToCbetOpp && e.toCall > 0) {
        f.foldToCbetOpp = true;
        s.foldToCbetOpp++;
        if (e.type == Act::Fold) s.foldToCbet++;
      }
      if (e.type == Act::Bet || e.type == Act::Raise) {
        if (e.type == Act::Raise) cbetActive = false;
        flopBetSeen = true;
      }
    }
    if (e.toCall > 0) {
      s.facingBet++;
      if (e.type == Act::Fold) s.foldVsBet++;
    }
    s.postActs++;
    if (e.type == Act::Bet || e.type == Act::Raise) s.aggActs++;
    if (e.type == Act::Call) s.callActs++;
  }
  bool reachedFlop = h.board.size() >= 3;
  if (reachedFlop) {
    std::set<int> foldedPre;
    for (auto& e : h.log)
      if (e.street == Street::Preflop && e.type == Act::Fold) foldedPre.insert(e.seat);
    for (int seat : h.order)
      if (!foldedPre.count(seat)) get(h.p(seat).id).sawFlop++;
  }
  if (!h.results.uncontested && reachedFlop) {
    for (auto& sd : h.results.showdown) {
      PlayerStats& st = get(h.p(sd.seat).id);
      st.wtsd++;
      auto it = h.results.winnings.find(sd.seat);
      if (it != h.results.winnings.end() && it->second > 0) st.wsd++;
    }
  }
  for (int seat : h.order) get(h.p(seat).id).net += h.p(seat).net;
}

}  // namespace hl
