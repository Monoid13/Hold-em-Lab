#pragma once
// Training drills: question generators + graders (pure logic; UI lives in src/ui).

#include <array>
#include <string>
#include <vector>

#include "core/table.hpp"

namespace hl {

enum class DrillKind { Preflop = 0, Equity, PotOdds, Outs, Showdown, Count };

struct DrillInfo {
  std::string key, title, desc;
};
const DrillInfo& drillInfo(DrillKind k);

struct DrillQuestion {
  DrillKind kind = DrillKind::Preflop;
  std::string prompt;
  // preflop: answers 0 = fold, 1 = call, 2 = raise
  std::string heroPos, openerPos, hand;
  double pct = 0;
  std::vector<int> choices;
  std::array<std::string, 3> choiceLabels{"Fold", "Call", "Raise"};
  std::array<double, 3> weights{};
  std::vector<std::string> notes;
  // cards
  std::vector<Card> hero, villain, board, outCards;
  std::vector<std::vector<Card>> hands;
  std::vector<int> scores;
  std::vector<std::string> names;
  Street street = Street::Preflop;
  // numeric answers
  double answer = 0;              // equity / pot-odds answer (0..1)
  int answerInt = 0;              // outs count, or showdown winner (-1 = split)
  std::vector<int> options;       // pot-odds multiple choice (percent)
  std::string explain;
};

struct DrillResult {
  bool correct = false;
  int score = 0;
  std::vector<std::string> explanation;
  int best = -1;
};

/** Synthetic 6-max 100bb preflop view (used by the preflop drill). */
View makePreflopView(const std::string& heroPos, const std::string& openerPos, std::array<Card, 2> hole, int stackBB = 100, int bb = 2);

DrillQuestion generateDrill(DrillKind kind, Rng& rng);
DrillResult gradeDrill(const DrillQuestion& q, int answer);

}  // namespace hl
