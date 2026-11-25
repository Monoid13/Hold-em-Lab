#pragma once
// Saved data: settings, long-term progress, hand histories and HUD stats (JSON files in the user's data dir).

#include <array>
#include <map>
#include <string>
#include <vector>

#include "app/json.hpp"
#include "bots/stats.hpp"
#include "coach/coach.hpp"
#include "core/table.hpp"

namespace hl {

std::string dataDir();
Json loadJson(const std::string& name);
bool saveJson(const std::string& name, const Json& j);
void clearAllData();

// ---------- settings ----------
struct Settings {
  std::string heroName = "You";
  int numOpponents = 5;
  std::string lineup = "mixed";
  std::vector<std::string> customSeats = {"shark", "station", "tag", "lag", "nit", "maniac", "scared", "tag"};
  int startingStackBB = 100;
  int bigBlind = 2;
  std::string mode = "cash";  // cash | tournament
  bool autoRebuy = true;
  int levelHands = 10;
  std::string speed = "normal";   // instant | fast | normal | realistic
  std::string coachMode = "all";  // all | mistakes | off
  bool showHud = true, showOppStats = true, revealProfiles = false, showAllCards = false, fourColor = false, autoDeal = true, allInEquity = true;
  bool revealInReview = true;

  double speedFactor() const;
  Json toJson() const;
  static Settings fromJson(const Json& j);
  static Settings load();
  void save() const;
};

/** Tournament blind schedule as multiples of the starting small blind: {sb, bb, ante}. */
const std::vector<std::array<int, 3>>& blindLevels();

// ---------- progress ----------
struct HandSummary {
  int id = 0;
  long long t = 0;
  double netBB = 0;
  std::string position;
  bool vpip = false, showdown = false;
};

struct DrillStat {
  int attempts = 0, correct = 0, streak = 0, bestStreak = 0;
  long scoreSum = 0;
};

struct PosStat {
  int hands = 0, vpip = 0;
  double netBB = 0;
};

struct Totals {
  int hands = 0, decisions = 0;
  double netBB = 0, bb100 = 0, accuracy = -1;
  std::array<int, 5> grades{};
};

class Progress {
 public:
  std::vector<HandSummary> hands;
  std::map<std::string, std::array<int, 5>> decisions;  // street -> counts per GradeLevel
  std::map<std::string, int> tags;
  double evLossBB = 0;
  std::map<std::string, DrillStat> drills;
  std::map<std::string, PosStat> byPosition;
  int sessions = 0;

  void load();
  void save() const;
  void reset();  // keeps drills
  void recordDecision(const Grade& g);
  void recordHand(const HandSummary& h);
  DrillStat& recordDrill(const std::string& key, bool correct, int score);
  Totals totals() const;
};

// ---------- history ----------
struct SnapPlayer {
  int stack = 0, bet = 0;
  bool folded = false, allIn = false;
};

struct Snapshot {
  std::string label;
  int seat = -1;
  Street street = Street::Preflop;
  std::vector<Card> board;
  int pot = 0;
  std::map<int, SnapPlayer> players;
  bool result = false;
  int potTotal() const;
};

struct RecPlayer {
  int seat = 0;
  std::string id, name, position, profile;
  int startStack = 0;
  std::array<Card, 2> hole{};
  unsigned color = 0;
};

struct RecDecision {
  int step = 0;
  Street street = Street::Preflop;
  GradeLevel grade = GradeLevel::Good;
  std::string heroLabel, bestLabel;
  bool hasEvLoss = false;
  double evLossBB = 0;
  std::vector<std::string> lines, tags;
};

struct HandRecord {
  std::string id;
  int handNo = 0;
  long long time = 0;
  int sb = 1, bb = 2, ante = 0, buttonSeat = 0, heroSeat = 0;
  std::vector<RecPlayer> players;
  std::vector<Snapshot> steps;
  std::vector<RecDecision> decisions;
  std::vector<Card> board;
  bool uncontested = true;
  std::map<int, int> winnings, nets;
  std::map<int, std::string> showdownNames;
  int heroNet() const;
  GradeLevel worstGrade(bool& any) const;
};

Snapshot snapshot(const Table& t, const std::string& label, int seat = -1);

class HistoryStore {
 public:
  std::vector<HandRecord> hands;
  void load();
  void save() const;
  void add(HandRecord rec);
  void clear();
};

// ---------- stats ----------
Json statsToJson(const StatsBook& s);
void statsFromJson(StatsBook& s, const Json& j);

}  // namespace hl
