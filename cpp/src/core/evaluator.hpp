#pragma once
// Fast 5-7 card hand evaluator. Higher score = better hand.
// Layout: category << 20 | r1 << 16 | r2 << 12 | r3 << 8 | r4 << 4 | r5

#include <string>
#include <vector>

#include "core/cards.hpp"

namespace hl {

enum Category { HIGH_CARD = 0, PAIR, TWO_PAIR, TRIPS, STRAIGHT, FLUSH, FULL_HOUSE, QUADS, STRAIGHT_FLUSH };

extern const char* const CATEGORY_NAMES[9];

struct EvalTables {
  signed char straightHigh[8192];
  int top5[8192];
  unsigned char popcount[8192];
  signed char highest[8192];
};
const EvalTables& evalTables();

int evaluate(const Card* cards, int n);
inline int evaluate(const std::vector<Card>& cards) { return evaluate(cards.data(), static_cast<int>(cards.size())); }
inline int categoryOf(int score) { return score >> 20; }
std::string describeScore(int score);

struct BestFive {
  std::vector<Card> cards;
  int score;
};
BestFive bestFive(const std::vector<Card>& cards);

}  // namespace hl
