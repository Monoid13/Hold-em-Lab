#pragma once
// Bayesian hand-range estimation from public actions. Bots and the coach read opponents only through
// this model: never through hidden cards.

#include <map>
#include <memory>
#include <vector>

#include "bots/stats.hpp"
#include "core/ranges.hpp"
#include "core/table.hpp"

namespace hl {

using Strengths = std::array<float, NUM_COMBOS>;  // -1 = invalid combo

double posFactor(const std::string& position);

/** Strength of each combo of a range on a board, in [0,1] relative to the range (draws get credit). */
Strengths computeStrengths(const Range& weights, const std::vector<Card>& board, const std::vector<Card>& dead = {});

/** Keep the strongest `q` fraction of a range's weight mass. */
Range truncateTop(const Range& weights, const Strengths& strengths, double q);

class RangeTracker {
 public:
  RangeTracker(const Hand& h, const StatsBook& stats);
  void observe(const LogEntry& e, const Hand& h);
  const Range& range(int seat) const { return ranges_.at(seat); }
  double width(int seat) const { return comboWidth(ranges_.at(seat)); }
  const Strengths& strengths(int seat, const std::vector<Card>& board, const std::vector<Card>& dead = {});
  const StatsBook& stats() const { return stats_; }

 private:
  const StatsBook& stats_;
  std::map<int, Range> ranges_;
  std::map<int, int> version_;
  std::map<std::string, std::shared_ptr<Strengths>> cache_;
  void multiplyClass(int seat, const ClassWeights& L);
  void normalize(int seat, float max);
  void observePreflop(const LogEntry& e, const Hand& h);
  void observePostflop(const LogEntry& e, const Hand& h);
};

}  // namespace hl
