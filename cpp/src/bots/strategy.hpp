#pragma once
// Decision core shared by the bots and the coach.
// Preflop: position-aware charts, range-vs-range thresholds and EV for stack-committing spots.
// Postflop: equity vs estimated ranges, fold equity from observed tendencies, EV per action.

#include <string>
#include <vector>

#include "bots/profiles.hpp"
#include "bots/range_model.hpp"
#include "core/table.hpp"

namespace hl {

struct Option {
  Act type = Act::Fold;
  int amount = 0;
  double w = 0;   // probability in the mixed strategy
  double ev = 0;  // chips, relative to folding now
  bool hasEv = false;
  double foldAll = -1, eqCalled = -1;
};

struct PreflopCtx {
  int raises = 0, limpers = 0, callersAfterRaise = 0, openerSeat = -1, threeBettorSeat = -1, lastRaiserSeat = -1, lastRaiseTo = 0;
  bool iRaised = false;
};

struct PreflopMeta {
  std::string hand, position;
  double pct = 1, effBB = 100, width = -1, equity = -1, needed = -1;
  bool inChart = false;
  PreflopCtx ctx;
};

struct PreflopResult {
  std::vector<Option> options;
  std::vector<std::string> notes;
  PreflopMeta meta;
  bool evBased = false;
};

struct PostflopResult {
  std::vector<Option> options;
  double equity = 0, perceived = 0, real = 1, needed = 0, percNoise = 0, adapt = 1;
  bool ip = false;
  int numOpp = 0, iterations = 700, maxReach = 0;
  // context kept so further bet sizes can be evaluated later (e.g. the hero's custom size)
  View view;
  std::vector<int> oppSeats;
  std::vector<Range> ranges;
  std::vector<Strengths> strengths;
  const StatsBook* stats = nullptr;
};

PreflopCtx preflopContext(const View& v);
bool isInPosition(const View& v, int vsSeat = -1);
int effectiveStack(const View& v);
std::vector<const ViewPlayer*> liveOpponents(const View& v);

PreflopResult preflopStrategy(const View& v, RangeTracker* tracker, const Profile& prof, Rng& rng, int iterations = 500, double tilt = 0);
PostflopResult postflopOptions(const View& v, RangeTracker& tracker, const Profile& prof, Rng& rng, int iterations = 700, double tilt = 0);
Option evalAmount(const PostflopResult& ctx, int to, Rng& rng);
std::vector<Option> chooseWeights(std::vector<Option> options, const Profile& prof, double pot, double tilt = 0);
double realization(Street s, bool ip, int nOpp);

inline bool isAggressive(Act a) { return a == Act::Bet || a == Act::Raise; }

}  // namespace hl
