#pragma once
// Bot decision-making: the strategy core plus human-like behaviour — mixed strategies, tilt,
// bet-sizing tells, rounding to "human" amounts and realistic think time.

#include <string>

#include "bots/profiles.hpp"
#include "bots/range_model.hpp"
#include "bots/strategy.hpp"
#include "core/table.hpp"

namespace hl {

struct BotMemory {
  double tilt = 0;
  void afterHand(double netBB, bool badBeat, const Profile& prof);
};

struct Decision {
  Action action;
  int thinkMs = 500;
  std::string chat;
};

Decision decideBot(const Table& t, int seat, const Profile& prof, BotMemory* mem, RangeTracker& tracker, Rng& rng, double iterationsScale = 1.0);
int humanRound(double amount, int bb, Rng& rng, double skill);
const Option& sampleOption(const std::vector<Option>& options, Rng& rng);
std::string randomChat(const std::string& kind, Rng& rng);

}  // namespace hl
