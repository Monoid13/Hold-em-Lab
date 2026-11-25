#pragma once
// Card primitives. A card is an int 0..51: rank = c >> 2 (0 = deuce .. 12 = ace), suit = c & 3.

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace hl {

using Card = int;

constexpr const char* RANKS = "23456789TJQKA";
constexpr const char* SUITS = "cdhs";

inline int rankOf(Card c) { return c >> 2; }
inline int suitOf(Card c) { return c & 3; }
inline Card makeCard(int rank, int suit) { return (rank << 2) | suit; }

std::string cardStr(Card c);                       // "As"
std::string cardPretty(Card c);                    // "A♠" (UTF-8)
std::string cardsPretty(const std::vector<Card>& cs);
const char* suitSymbol(int suit);                  // UTF-8 suit glyph
const char* rankName(int r);                       // "Ace"
const char* rankPlural(int r);                     // "Aces"
std::string rankLabel(int r);                      // "A", "10", ...
Card parseCard(const std::string& s);
std::vector<Card> parseCards(const std::string& s);  // "As Kd" or "AsKd"

/** Fast PRNG (xoshiro256**) with a uniform double helper. */
class Rng {
 public:
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull);
  static Rng fromEntropy();
  uint64_t nextU64();
  double next();             // [0, 1)
  int below(int n);          // [0, n)
  using result_type = uint64_t;
  static constexpr uint64_t min() { return 0; }
  static constexpr uint64_t max() { return ~0ull; }
  uint64_t operator()() { return nextU64(); }

 private:
  uint64_t s_[4];
};

template <typename T>
void shuffleVec(std::vector<T>& v, Rng& rng) {
  for (int i = static_cast<int>(v.size()) - 1; i > 0; --i) {
    int j = rng.below(i + 1);
    std::swap(v[i], v[j]);
  }
}

std::vector<Card> fullDeck();

}  // namespace hl
