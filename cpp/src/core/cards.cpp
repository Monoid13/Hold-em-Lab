#include "core/cards.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace hl {

static const char* SUIT_SYM[4] = {"\xE2\x99\xA3", "\xE2\x99\xA6", "\xE2\x99\xA5", "\xE2\x99\xA0"};  // ♣ ♦ ♥ ♠
static const char* RANK_NAME[13] = {"Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Jack", "Queen", "King", "Ace"};
static const char* RANK_PLURAL[13] = {"Twos", "Threes", "Fours", "Fives", "Sixes", "Sevens", "Eights", "Nines", "Tens", "Jacks", "Queens", "Kings", "Aces"};

std::string cardStr(Card c) { return std::string(1, RANKS[c >> 2]) + SUITS[c & 3]; }
std::string rankLabel(int r) { return r == 8 ? "10" : std::string(1, RANKS[r]); }
std::string cardPretty(Card c) { return std::string(1, RANKS[c >> 2]) + SUIT_SYM[c & 3]; }
std::string cardsPretty(const std::vector<Card>& cs) {
  std::string out;
  for (size_t i = 0; i < cs.size(); ++i) {
    if (i) out += ' ';
    out += cardPretty(cs[i]);
  }
  return out;
}
const char* suitSymbol(int s) { return SUIT_SYM[s & 3]; }
const char* rankName(int r) { return RANK_NAME[r]; }
const char* rankPlural(int r) { return RANK_PLURAL[r]; }

Card parseCard(const std::string& s) {
  if (s.size() < 2) throw std::invalid_argument("bad card");
  const char* rp = std::strchr(RANKS, std::toupper(static_cast<unsigned char>(s[0])));
  const char* sp = std::strchr(SUITS, std::tolower(static_cast<unsigned char>(s[1])));
  if (!rp || !sp || !*rp || !*sp) throw std::invalid_argument("bad card: " + s);
  return makeCard(static_cast<int>(rp - RANKS), static_cast<int>(sp - SUITS));
}

std::vector<Card> parseCards(const std::string& s) {
  std::vector<Card> out;
  std::string tok;
  for (char ch : s) {
    if (ch == ' ' || ch == ',') continue;
    tok += ch;
    if (tok.size() == 2) {
      out.push_back(parseCard(tok));
      tok.clear();
    }
  }
  return out;
}

std::vector<Card> fullDeck() {
  std::vector<Card> d(52);
  for (int i = 0; i < 52; ++i) d[i] = i;
  return d;
}

static uint64_t splitmix(uint64_t& x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

Rng::Rng(uint64_t seed) {
  uint64_t x = seed;
  for (auto& v : s_) v = splitmix(x);
}

Rng Rng::fromEntropy() {
  std::random_device rd;
  uint64_t seed = (static_cast<uint64_t>(rd()) << 32) ^ rd();
  seed ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return Rng(seed);
}

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint64_t Rng::nextU64() {
  const uint64_t result = rotl(s_[1] * 5, 7) * 9;
  const uint64_t t = s_[1] << 17;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 45);
  return result;
}

double Rng::next() { return static_cast<double>(nextU64() >> 11) * 0x1.0p-53; }
int Rng::below(int n) { return static_cast<int>(next() * n); }

}  // namespace hl
