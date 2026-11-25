#include "core/evaluator.hpp"

#include <algorithm>

namespace hl {

const char* const CATEGORY_NAMES[9] = {"High Card", "Pair", "Two Pair", "Three of a Kind", "Straight", "Flush", "Full House", "Four of a Kind", "Straight Flush"};

static EvalTables buildTables() {
  EvalTables t{};
  for (int m = 0; m < 8192; ++m) {
    int pc = 0, enc = 0, taken = 0, hi = -1;
    for (int r = 12; r >= 0; --r) {
      if (m & (1 << r)) {
        ++pc;
        if (hi < 0) hi = r;
        if (taken < 5) {
          enc |= r << (4 * (4 - taken));
          ++taken;
        }
      }
    }
    t.popcount[m] = static_cast<unsigned char>(pc);
    t.top5[m] = enc;
    t.highest[m] = static_cast<signed char>(hi);
    t.straightHigh[m] = -1;
    for (int h = 12; h >= 4; --h) {
      int sm = 0x1f << (h - 4);
      if ((m & sm) == sm) {
        t.straightHigh[m] = static_cast<signed char>(h);
        break;
      }
    }
    if (t.straightHigh[m] < 0 && (m & 0x100f) == 0x100f) t.straightHigh[m] = 3;
  }
  return t;
}

const EvalTables& evalTables() {
  static const EvalTables tables = buildTables();
  return tables;
}

int evaluate(const Card* cards, int n) {
  const EvalTables& T = evalTables();
  int sm[4] = {0, 0, 0, 0};
  int m1 = 0, m2 = 0, m3 = 0, m4 = 0;
  for (int i = 0; i < n; ++i) {
    const int c = cards[i];
    const int b = 1 << (c >> 2);
    sm[c & 3] |= b;
    m4 |= m3 & b;
    m3 |= m2 & b;
    m2 |= m1 & b;
    m1 |= b;
  }
  for (int s = 0; s < 4; ++s) {
    if (T.popcount[sm[s]] >= 5) {
      int sf = T.straightHigh[sm[s]];
      if (sf >= 0) return (STRAIGHT_FLUSH << 20) | (sf << 16);
      return (FLUSH << 20) | T.top5[sm[s]];  // flush can't coexist with quads/boat in 7 cards
    }
  }
  if (m4) {
    int q = T.highest[m4];
    int k = T.highest[m1 & ~(1 << q)];
    return (QUADS << 20) | (q << 16) | (std::max(k, 0) << 12);
  }
  if (m3) {
    int t = T.highest[m3];
    int rest = m2 & ~(1 << t);
    if (rest) return (FULL_HOUSE << 20) | (t << 16) | (T.highest[rest] << 12);
  }
  int st = T.straightHigh[m1];
  if (st >= 0) return (STRAIGHT << 20) | (st << 16);
  if (m3) {
    int t = T.highest[m3];
    return (TRIPS << 20) | (t << 16) | ((T.top5[m1 & ~(1 << t)] >> 4) & 0xff00);
  }
  if (m2) {
    int p1 = T.highest[m2];
    int rem = m2 & ~(1 << p1);
    if (rem) {
      int p2 = T.highest[rem];
      int k = T.highest[m1 & ~(1 << p1) & ~(1 << p2)];
      return (TWO_PAIR << 20) | (p1 << 16) | (p2 << 12) | (std::max(k, 0) << 8);
    }
    return (PAIR << 20) | (p1 << 16) | ((T.top5[m1 & ~(1 << p1)] >> 4) & 0xfff0);
  }
  return T.top5[m1];
}

static int nib(int score, int i) { return (score >> (16 - 4 * i)) & 15; }

std::string describeScore(int score) {
  int a = nib(score, 0), b = nib(score, 1);
  switch (categoryOf(score)) {
    case HIGH_CARD: return std::string(rankName(a)) + " high";
    case PAIR: return std::string("Pair of ") + rankPlural(a);
    case TWO_PAIR: return std::string("Two Pair, ") + rankPlural(a) + " and " + rankPlural(b);
    case TRIPS: return std::string("Three of a Kind, ") + rankPlural(a);
    case STRAIGHT: return std::string("Straight, ") + rankName(a) + " high";
    case FLUSH: return std::string("Flush, ") + rankName(a) + " high";
    case FULL_HOUSE: return std::string("Full House, ") + rankPlural(a) + " full of " + rankPlural(b);
    case QUADS: return std::string("Four of a Kind, ") + rankPlural(a);
    case STRAIGHT_FLUSH: return a == 12 ? "Royal Flush" : std::string("Straight Flush, ") + rankName(a) + " high";
  }
  return "Unknown";
}

BestFive bestFive(const std::vector<Card>& cards) {
  const int n = static_cast<int>(cards.size());
  if (n <= 5) return {cards, evaluate(cards)};
  BestFive best{{}, -1};
  Card pick[5];
  for (int a = 0; a < n; ++a)
    for (int b = a + 1; b < n; ++b)
      for (int c = b + 1; c < n; ++c)
        for (int d = c + 1; d < n; ++d)
          for (int e = d + 1; e < n; ++e) {
            pick[0] = cards[a]; pick[1] = cards[b]; pick[2] = cards[c]; pick[3] = cards[d]; pick[4] = cards[e];
            int s = evaluate(pick, 5);
            if (s > best.score) {
              best.score = s;
              best.cards.assign(pick, pick + 5);
            }
          }
  return best;
}

}  // namespace hl
