#pragma once
// The coach analyses your spot with the same information you have (public actions + your cards),
// recommends an action and grades what you did by estimated EV loss.

#include <string>
#include <utility>
#include <vector>

#include "bots/strategy.hpp"
#include "core/handinfo.hpp"

namespace hl {

enum class GradeLevel { Best = 0, Good, Inaccuracy, Mistake, Blunder };
const char* gradeLabel(GradeLevel g);
const char* gradeSymbol(GradeLevel g);
double gradeScore(GradeLevel g);

struct LeakTag {
  std::string key, label, tip;
};
const std::vector<LeakTag>& leakTags();
const LeakTag* leakTag(const std::string& key);

struct OppRangeInfo {
  int seat;
  std::string name;
  double width;
};

struct Spot {
  View view;
  HandAnalysis info;
  int pot = 0, toCall = 0, cardsToCome = 0;
  double needed = 0, spr = 0, equity = 0, bestEV = 0;
  bool ip = false, isPreflop = true;
  std::vector<OppRangeInfo> oppRanges;
  PreflopResult preflop;
  PostflopResult postflop;
  Action recommended;
  std::string recommendedLabel;
  std::vector<std::pair<std::string, double>> mix;  // preflop strategy mix
};

struct Grade {
  GradeLevel grade = GradeLevel::Good;
  bool hasEvLoss = false;
  double evLoss = 0, evLossBB = 0;
  std::string heroLabel, bestLabel;
  Street street = Street::Preflop;
  std::vector<std::string> lines, tags;
  double equity = 0, needed = 0;
};

/** Plain-language guidance for beginners (no jargon). */
struct PlainAdvice {
  int level = 0;              // 0 weak .. 4 very strong
  std::string strengthWord;   // "Weak", "So-so", "Decent", "Strong", "Very strong"
  std::string handLine;       // "You have a Pair of Kings (top pair)."
  std::string suggestion;     // "Call 6 - match the bet"
  std::vector<std::string> reasons;
};
PlainAdvice beginnerAdvice(const Spot& spot);
/** One-line verdict on a graded decision, e.g. "Mistake: folding was better here." */
std::string plainSummary(const Grade& g);
/** Short explanation of what an action does, e.g. "match the bet". */
std::string plainAction(Act type);

Spot analyzeSpot(const Table& t, int seat, RangeTracker& tracker, Rng& rng, int iterations = 1600);
Grade gradeDecision(const Spot& spot, Action action);
std::string actionLabel(Act type, int amount, const View& v);
std::string sizeName(double frac);
std::string fmtPct(double x, int digits = 0);
std::string fmtChips(double x);  // signed: +12 / −4

}  // namespace hl
