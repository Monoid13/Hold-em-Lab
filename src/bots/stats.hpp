#pragma once
// Player statistics (HUD stats) built purely from public hand logs.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/table.hpp"

namespace hl {

struct PlayerStats {
  int hands = 0, vpip = 0, pfr = 0, threeBetOpp = 0, threeBet = 0, foldTo3betOpp = 0, foldTo3bet = 0;
  int stealOpp = 0, steal = 0, cbetOpp = 0, cbet = 0, foldToCbetOpp = 0, foldToCbet = 0;
  int facingBet = 0, foldVsBet = 0, postActs = 0, aggActs = 0, callActs = 0, sawFlop = 0, wtsd = 0, wsd = 0;
  long net = 0;
};

struct StatDef {
  const char* key;
  const char* label;
  double prior;
  double strength;
  int PlayerStats::*opp;
  int PlayerStats::*hit;
  const char* desc;
};

const std::vector<StatDef>& statDefs();
const StatDef& statDef(const std::string& key);

struct RawStat {
  int n = 0, k = 0;
  std::optional<double> pct;
};

class StatsBook {
 public:
  std::map<std::string, PlayerStats> players;

  PlayerStats& get(const std::string& id) { return players[id]; }
  const PlayerStats& peek(const std::string& id) const;
  double rate(const std::string& id, const std::string& stat) const;  // Bayesian-shrunk
  RawStat raw(const std::string& id, const std::string& stat) const;  // observed
  std::optional<double> af(const std::string& id) const;               // aggression factor; inf if no calls
  void recordHand(const Hand& h);
  void reset(const std::string& id) { players.erase(id); }
  void resetAll() { players.clear(); }
};

}  // namespace hl
