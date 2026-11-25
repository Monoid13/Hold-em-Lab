#pragma once
// Equity: Monte Carlo vs weighted ranges / random hands, exact enumeration for known hands.

#include <array>
#include <vector>

#include "core/ranges.hpp"

namespace hl {

struct Opponent {
  const Range* range = nullptr;  // nullptr + !known => random hand
  bool known = false;
  std::array<Card, 2> cards{};
};

struct EquityResult {
  double equity = 0.5, win = 0, tie = 0;
  int samples = 0;
};

EquityResult calcEquity(const std::array<Card, 2>& hero, const std::vector<Card>& board, const std::vector<Opponent>& opps, int iterations, Rng& rng);

/** Equity of each known hand. Exact when <= 2 board cards remain, otherwise Monte Carlo. */
std::vector<double> knownEquity(const std::vector<std::array<Card, 2>>& hands, const std::vector<Card>& board, int iterations, Rng& rng);

}  // namespace hl
