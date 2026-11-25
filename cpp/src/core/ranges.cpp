#include "core/ranges.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace hl {

static const char* RANKING_TEXT =
    "AA KK QQ AKs JJ AQs KQs AJs AKo TT "
    "KJs ATs QJs AQo KTs 99 QTs JTs A9s AJo "
    "KQo 88 A8s K9s T9s A7s Q9s J9s A5s ATo "
    "KJo A6s A4s 77 QJo A3s K8s KTo T8s 98s "
    "A2s J8s Q8s K7s 66 QTo JTo A9o 87s K6s "
    "97s T7s K5s 55 76s Q7s K4s A8o 86s J7s "
    "65s K3s Q6s 44 K9o 96s K2s Q5s 54s 75s "
    "T9o A7o Q4s J9o Q9o A5o 33 T6s J6s 85s "
    "Q3s 64s A6o 95s J5s Q2s A4o 22 74s 53s "
    "J4s K8o T8o 98o J8o A3o T5s 84s 63s J3s "
    "43s A2o K7o 94s J2s T4s 73s Q8o 52s 87o "
    "T3s 97o 93s K6o 62s 42s T2s 83s 92s 76o "
    "K5o 82s J7o 32s 86o T7o 72s Q7o K4o 65o "
    "96o K3o 75o Q6o 54o J6o 85o K2o T6o 64o "
    "Q5o 95o 53o 74o Q4o J5o 43o 84o Q3o J4o "
    "63o T5o 94o Q2o J3o 73o 52o T4o J2o 93o "
    "42o 83o T3o 62o 92o 32o T2o 82o 72o";

static RangeData build() {
  RangeData d;
  for (int row = 0; row < 13; ++row) {
    for (int col = 0; col < 13; ++col) {
      int rr = 12 - row, cr = 12 - col;
      HandClass c;
      c.index = row * 13 + col;
      c.row = row;
      c.col = col;
      if (row == col) {
        c.name = std::string(1, RANKS[rr]) + RANKS[rr];
        c.type = ClassType::Pair;
        c.combos = 6;
      } else if (col > row) {
        c.name = std::string(1, RANKS[rr]) + RANKS[cr] + "s";
        c.type = ClassType::Suited;
        c.combos = 4;
      } else {
        c.name = std::string(1, RANKS[cr]) + RANKS[rr] + "o";
        c.type = ClassType::Offsuit;
        c.combos = 12;
      }
      c.hi = std::max(rr, cr);
      c.lo = std::min(rr, cr);
      d.byName[c.name] = c.index;
      d.classes.push_back(c);
    }
  }
  d.comboIndex.fill(-1);
  d.classCombos.assign(NUM_CLASSES, {});
  int k = 0;
  for (int a = 1; a < 52; ++a)
    for (int b = 0; b < a; ++b) {
      d.c1[k] = static_cast<unsigned char>(a);
      d.c2[k] = static_cast<unsigned char>(b);
      int cls = classOf(a, b);
      d.comboClass[k] = static_cast<unsigned char>(cls);
      d.comboIndex[a * 52 + b] = static_cast<short>(k);
      d.comboIndex[b * 52 + a] = static_cast<short>(k);
      d.classCombos[cls].push_back(k);
      ++k;
    }
  std::istringstream ss(RANKING_TEXT);
  std::string tok;
  while (ss >> tok) {
    auto it = d.byName.find(tok);
    if (it == d.byName.end()) throw std::runtime_error("bad ranking token " + tok);
    d.ranking.push_back(it->second);
  }
  if (d.ranking.size() != NUM_CLASSES) throw std::runtime_error("ranking incomplete");
  double cum = 0;
  for (size_t i = 0; i < d.ranking.size(); ++i) {
    int idx = d.ranking[i];
    cum += d.classes[idx].combos;
    d.percentile[idx] = static_cast<float>(cum / NUM_COMBOS);
    d.rankPos[idx] = static_cast<short>(i);
  }
  return d;
}

const RangeData& R() {
  static const RangeData data = build();
  return data;
}

int classOf(Card c1, Card c2) {
  int r1 = c1 >> 2, r2 = c2 >> 2;
  int hi = std::max(r1, r2), lo = std::min(r1, r2);
  if (hi == lo) return (12 - hi) * 14;
  if ((c1 & 3) == (c2 & 3)) return (12 - hi) * 13 + (12 - lo);
  return (12 - lo) * 13 + (12 - hi);
}

static int rankIdx(char ch) {
  const char* p = std::strchr(RANKS, std::toupper(static_cast<unsigned char>(ch)));
  if (!p || !*p) throw std::invalid_argument("bad rank");
  return static_cast<int>(p - RANKS);
}

static void addClass(ClassSet& set, int hi, int lo, char type) {
  if (hi == lo) {
    set.set((12 - hi) * 14);
    return;
  }
  if (type != 'o') set.set((12 - hi) * 13 + (12 - lo));
  if (type != 's') set.set((12 - lo) * 13 + (12 - hi));
}

ClassSet parseRange(const std::string& str) {
  ClassSet set;
  std::string norm = str;
  std::replace(norm.begin(), norm.end(), ',', ' ');
  std::istringstream ss(norm);
  std::string t;
  while (ss >> t) {
    std::string a = t, b;
    auto dash = t.find('-');
    if (dash != std::string::npos) {
      a = t.substr(0, dash);
      b = t.substr(dash + 1);
    }
    bool plus = !a.empty() && a.back() == '+';
    if (plus) a.pop_back();
    if (a.size() < 2) throw std::invalid_argument("bad range token " + t);
    int r1 = rankIdx(a[0]), r2 = rankIdx(a[1]);
    char type = a.size() > 2 ? static_cast<char>(std::tolower(a[2])) : 0;
    if (r1 == r2) {
      if (plus)
        for (int r = r1; r <= 12; ++r) addClass(set, r, r, 0);
      else if (!b.empty()) {
        int e = rankIdx(b[0]);
        for (int r = std::min(r1, e); r <= std::max(r1, e); ++r) addClass(set, r, r, 0);
      } else
        addClass(set, r1, r1, 0);
      continue;
    }
    if (r2 > r1) std::swap(r1, r2);
    if (plus)
      for (int k = r2; k < r1; ++k) addClass(set, r1, k, type);
    else if (!b.empty()) {
      int k2 = rankIdx(b[1]);
      for (int k = std::min(r2, k2); k <= std::max(r2, k2); ++k) addClass(set, r1, k, type);
    } else
      addClass(set, r1, r2, type);
  }
  return set;
}

double rangeWidth(const ClassSet& set) {
  int c = 0;
  for (int i = 0; i < NUM_CLASSES; ++i)
    if (set.test(i)) c += R().classes[i].combos;
  return static_cast<double>(c) / NUM_COMBOS;
}

std::vector<std::string> positionLabels(int n) {
  if (n == 2) return {"BTN", "BB"};
  if (n == 3) return {"BTN", "SB", "BB"};
  static const char* early[] = {"UTG", "UTG+1", "UTG+2", "LJ", "HJ", "CO"};
  std::vector<std::string> out = {"BTN", "SB", "BB"};
  for (int i = 6 - (n - 3); i < 6; ++i) out.push_back(early[i]);
  return out;
}

struct Chart {
  std::string key, text;
  ClassSet set;
  double width;
};

static const std::vector<Chart>& charts() {
  static const std::vector<Chart> cs = [] {
    std::vector<std::pair<std::string, std::string>> raw = {
        {"UTG", "55+, A9s+, A5s, KTs+, QTs+, JTs, T9s, AJo+, KQo"},
        {"UTG+1", "44+, A8s+, A5s-A4s, KTs+, QTs+, JTs, T9s, 98s, ATo+, KQo"},
        {"UTG+2", "33+, A7s+, A5s-A4s, K9s+, QTs+, JTs, T9s, 98s, 87s, ATo+, KJo+"},
        {"LJ", "22+, A2s+, K9s+, QTs+, JTs, T9s, 98s, 87s, 76s, 65s, ATo+, KJo+"},
        {"HJ", "22+, A2s+, K7s+, Q9s+, J9s+, T8s+, 98s, 87s, 76s, 65s, 54s, ATo+, KTo+, QJo"},
        {"CO", "22+, A2s+, K5s+, Q8s+, J8s+, T8s+, 97s+, 86s+, 75s+, 65s, 54s, A8o+, A5o, KTo+, QTo+, JTo"},
        {"BTN", "22+, A2s+, K2s+, Q4s+, J6s+, T6s+, 96s+, 85s+, 74s+, 64s+, 53s+, 43s, A2o+, K8o+, Q9o+, J9o+, T8o+, 98o, 87o"},
        {"SB", "22+, A2s+, K4s+, Q6s+, J7s+, T7s+, 96s+, 86s+, 75s+, 64s+, 54s, A5o+, K9o+, Q9o+, J9o+, T9o"},
        {"HU", "22+, A2s+, K2s+, Q2s+, J2s+, T3s+, 95s+, 85s+, 74s+, 63s+, 53s+, 43s, A2o+, K4o+, Q6o+, J7o+, T7o+, 97o+, 87o, 76o, 65o"},
    };
    std::vector<Chart> out;
    for (auto& [k, t] : raw) {
      Chart c{k, t, parseRange(t), 0};
      c.width = rangeWidth(c.set);
      out.push_back(c);
    }
    return out;
  }();
  return cs;
}

const std::vector<std::string>& rfiPositions() {
  static const std::vector<std::string> p = {"UTG", "UTG+1", "UTG+2", "LJ", "HJ", "CO", "BTN", "SB", "HU"};
  return p;
}

const ClassSet* rfiSet(const std::string& key) {
  for (auto& c : charts())
    if (c.key == key) return &c.set;
  return nullptr;
}

double rfiWidth(const std::string& key) {
  for (auto& c : charts())
    if (c.key == key) return c.width;
  return 0.2;
}

std::string rfiChartText(const std::string& key) {
  for (auto& c : charts())
    if (c.key == key) return c.text;
  return "";
}

std::string rfiKey(const std::string& position, int n) {
  if (n == 2 && position == "BTN") return "HU";
  return rfiSet(position) ? position : "";
}

double softIn(double pct, double width, double softness) {
  if (width <= 0) return 0;
  double s = softness * std::max(0.3, std::min(1.5, width / 0.2)) + 0.004;
  double v = (width - pct) / s + 0.5;
  return v <= 0 ? 0 : v >= 1 ? 1 : v;
}

double comboWidth(const Range& r) {
  double s = 0;
  for (float w : r) s += w;
  return s / NUM_COMBOS;
}

Range fullRange(float v) {
  Range r;
  r.fill(v);
  return r;
}

ClassWeights combosToClassWeights(const Range& r) {
  ClassWeights out{};
  for (int i = 0; i < NUM_CLASSES; ++i) {
    const auto& list = R().classCombos[i];
    double s = 0;
    for (int k : list) s += r[k];
    out[i] = static_cast<float>(s / list.size());
  }
  return out;
}

}  // namespace hl
