#include "bots/range_model.hpp"

#include <algorithm>
#include <numeric>

#include "core/evaluator.hpp"

namespace hl {

namespace {
double clampd(double x, double a, double b) { return x < a ? a : x > b ? b : x; }

double soft(double pct, double width, double s = 0.03) {
  double v = (width - pct) / (s * std::max(0.4, width / 0.2) + 0.006) + 0.5;
  return v <= 0 ? 0 : v >= 1 ? 1 : v;
}

const std::array<float, NUM_CLASSES>& suitedPlayable() {
  static const std::array<float, NUM_CLASSES> t = [] {
    std::array<float, NUM_CLASSES> a{};
    for (auto& c : R().classes) {
      if (c.type == ClassType::Suited && (c.hi == 12 || c.hi - c.lo <= 2)) a[c.index] = 1;
      if (c.type == ClassType::Pair) a[c.index] = 0.6f;
    }
    return a;
  }();
  return t;
}
}  // namespace

double posFactor(const std::string& p) {
  static const std::map<std::string, double> f = {{"UTG", 0.55}, {"UTG+1", 0.62}, {"UTG+2", 0.7}, {"LJ", 0.8}, {"HJ", 0.95},
                                                  {"CO", 1.25},  {"BTN", 1.9},   {"SB", 1.4},    {"BB", 0.7}};
  auto it = f.find(p);
  return it == f.end() ? 1.0 : it->second;
}

Strengths computeStrengths(const Range& w, const std::vector<Card>& board, const std::vector<Card>& dead) {
  Strengths out;
  out.fill(-1.f);
  const auto& D = R();
  bool blocked[52] = {};
  for (Card c : board) blocked[c] = true;
  for (Card c : dead) blocked[c] = true;
  if (board.size() < 3) {
    for (int k = 0; k < NUM_COMBOS; ++k)
      if (w[k] > 0 && !blocked[D.c1[k]] && !blocked[D.c2[k]]) out[k] = 1.f - D.percentile[D.comboClass[k]];
    return out;
  }
  const int n = static_cast<int>(board.size());
  Card buf[7];
  for (int i = 0; i < n; ++i) buf[2 + i] = board[i];
  std::vector<int> idx;
  std::vector<int> scores(NUM_COMBOS, 0);
  double total = 0;
  for (int k = 0; k < NUM_COMBOS; ++k) {
    if (w[k] <= 0 || blocked[D.c1[k]] || blocked[D.c2[k]]) continue;
    buf[0] = D.c1[k];
    buf[1] = D.c2[k];
    scores[k] = evaluate(buf, n + 2);
    idx.push_back(k);
    total += w[k];
  }
  if (idx.empty()) return out;
  std::sort(idx.begin(), idx.end(), [&](int a, int b) { return scores[a] < scores[b]; });
  double cum = 0;
  for (size_t i = 0; i < idx.size();) {
    size_t j = i;
    double groupW = 0;
    while (j < idx.size() && scores[idx[j]] == scores[idx[i]]) groupW += w[idx[j++]];
    float pct = static_cast<float>((cum + groupW / 2) / total);
    for (size_t t = i; t < j; ++t) out[idx[t]] = pct;
    cum += groupW;
    i = j;
  }
  if (n < 5) {
    const auto& T = evalTables();
    int bs[4] = {};
    int bm = 0;
    for (Card c : board) {
      bs[c & 3]++;
      bm |= 1 << (c >> 2);
    }
    for (int k : idx) {
      int a = D.c1[k], b = D.c2[k];
      double draw = 0;
      if (categoryOf(scores[k]) < FLUSH) {
        int sa = a & 3, sb = b & 3;
        int fa = bs[sa] + 1 + (sa == sb ? 1 : 0), fb = bs[sb] + 1 + (sa == sb ? 1 : 0);
        if (fa == 4 || fb == 4) draw = n == 3 ? 0.62 : 0.5;
      }
      int m = bm | (1 << (a >> 2)) | (1 << (b >> 2));
      if (T.straightHigh[m] < 0) {
        int outs = 0;
        for (int r = 0; r < 13; ++r) {
          if (m & (1 << r)) continue;
          if (T.straightHigh[m | (1 << r)] >= 0 && T.straightHigh[bm | (1 << r)] < 0) ++outs;
        }
        double sd = outs >= 2 ? (n == 3 ? 0.56 : 0.45) : outs == 1 ? (n == 3 ? 0.33 : 0.25) : 0;
        draw = (draw > 0 && sd > 0) ? std::min(0.85, draw + sd * 0.45) : std::max(draw, sd);
      }
      if (draw > 0) {
        double made = out[k];
        out[k] = static_cast<float>(made > draw ? std::min(1.0, made + draw * 0.15) : std::min(1.0, draw + made * 0.15));
      }
    }
  }
  return out;
}

Range truncateTop(const Range& w, const Strengths& s, double q) {
  Range out{};
  if (q >= 0.999) {
    for (int k = 0; k < NUM_COMBOS; ++k)
      if (s[k] >= 0) out[k] = w[k];
    return out;
  }
  std::vector<int> idx;
  double total = 0;
  for (int k = 0; k < NUM_COMBOS; ++k)
    if (s[k] >= 0 && w[k] > 0) {
      idx.push_back(k);
      total += w[k];
    }
  std::sort(idx.begin(), idx.end(), [&](int a, int b) { return s[a] > s[b]; });
  double target = std::max(q, 0.02) * total, cum = 0;
  for (int k : idx) {
    if (cum >= target) break;
    double take = std::min<double>(w[k], target - cum);
    out[k] = static_cast<float>(take);
    cum += take;
  }
  return out;
}

RangeTracker::RangeTracker(const Hand& h, const StatsBook& stats) : stats_(stats) {
  for (int seat : h.order) {
    ranges_[seat] = fullRange(1.f);
    version_[seat] = 0;
  }
}

const Strengths& RangeTracker::strengths(int seat, const std::vector<Card>& board, const std::vector<Card>& dead) {
  std::string key = std::to_string(seat) + "|" + std::to_string(version_[seat]) + "|";
  for (Card c : board) key += std::to_string(c) + ",";
  key += "|";
  for (Card c : dead) key += std::to_string(c) + ",";
  auto it = cache_.find(key);
  if (it != cache_.end()) return *it->second;
  if (cache_.size() > 64) cache_.clear();
  auto s = std::make_shared<Strengths>(computeStrengths(ranges_[seat], board, dead));
  cache_[key] = s;
  return *s;
}

void RangeTracker::normalize(int seat, float max) {
  Range& r = ranges_[seat];
  if (max <= 1e-9f) r.fill(0.2f);  // contradictory evidence: reset to a broad range
  else if (max < 0.5f) {
    float inv = 1.f / max;
    for (auto& v : r) v *= inv;
  }
  version_[seat]++;
}

void RangeTracker::multiplyClass(int seat, const ClassWeights& L) {
  Range& r = ranges_[seat];
  float max = 0;
  const auto& D = R();
  for (int k = 0; k < NUM_COMBOS; ++k) {
    r[k] *= L[D.comboClass[k]];
    max = std::max(max, r[k]);
  }
  normalize(seat, max);
}

void RangeTracker::observe(const LogEntry& e, const Hand& h) {
  if (e.blind || e.type == Act::Uncalled || e.type == Act::Fold) return;
  if (!ranges_.count(e.seat)) return;
  if (e.street == Street::Preflop) observePreflop(e, h);
  else observePostflop(e, h);
}

void RangeTracker::observePreflop(const LogEntry& e, const Hand& h) {
  const HandPlayer& hp = h.p(e.seat);
  const std::string& id = hp.id;
  double vpip = stats_.rate(id, "vpip"), pfr = stats_.rate(id, "pfr"), tb = stats_.rate(id, "threeBet");
  double pf = posFactor(hp.position);
  double hu = h.order.size() == 2 ? 1.8 : 1.0;
  const auto& P = R().percentile;
  const auto& SP = suitedPlayable();
  ClassWeights L{};
  int rc = e.raiseCount;
  if (e.type == Act::Raise) {
    double width;
    if (rc == 0) width = clampd(pfr * pf * hu, 0.03, 0.9);
    else if (rc == 1) width = clampd(tb * (pf > 1 ? 1.2 : 0.9) * hu, 0.02, 0.35);
    else width = clampd(tb * 0.4 * hu, 0.012, 0.1);
    if (e.allIn && e.to >= hp.startStack * 0.9 && rc == 0) width = clampd(width * 1.3, 0.04, 0.9);
    for (int i = 0; i < NUM_CLASSES; ++i) {
      double w = soft(P[i], width);
      if (rc >= 1 && P[i] > width) w = std::max(w, SP[i] * 0.18 * clampd(tb / 0.07, 0.3, 2.0));
      L[i] = static_cast<float>(w);
    }
  } else if (e.type == Act::Check) {
    double top = clampd(pfr * 0.6, 0.03, 0.3);
    for (int i = 0; i < NUM_CLASSES; ++i) L[i] = P[i] <= top ? 0.35f : 1.f;
  } else if (e.type == Act::Call) {
    double hi, lo, loW;
    if (rc == 0) {
      hi = clampd(vpip * pf * 1.1 * hu, 0.05, 0.95);
      lo = clampd(pfr * pf, 0.02, 0.5);
      loW = 0.3;
    } else if (rc == 1) {
      bool isBB = hp.position == "BB";
      double price = static_cast<double>(e.toCall) / std::max(1, e.potBefore + e.toCall);
      double bbBoost = isBB ? clampd(1.6 - price * 1.5, 1.0, 1.7) : 0.85;
      hi = clampd(vpip * pf * bbBoost * hu, 0.04, 0.9);
      lo = clampd(tb, 0.015, 0.2);
      loW = 0.35;
    } else {
      hi = clampd(tb * 1.4 + 0.025, 0.03, 0.22);
      lo = clampd(tb * 0.3, 0.01, 0.04);
      loW = 0.45;
    }
    for (int i = 0; i < NUM_CLASSES; ++i) L[i] = static_cast<float>(soft(P[i], hi) * (P[i] <= lo ? loW : 1.0));
    if (e.allIn && e.amount >= hp.startStack * 0.5) {
      double w = clampd(vpip * 0.45, 0.04, 0.5);
      for (int i = 0; i < NUM_CLASSES; ++i) L[i] = static_cast<float>(std::min<double>(L[i] + 0.0001, soft(P[i], w)));
    }
  } else
    return;
  multiplyClass(e.seat, L);
}

void RangeTracker::observePostflop(const LogEntry& e, const Hand& h) {
  const int seat = e.seat;
  const std::string& id = h.p(seat).id;
  std::vector<Card> board(h.board.begin(), h.board.begin() + std::min<size_t>(h.board.size(), boardCardsAt(e.street)));
  const Strengths s = strengths(seat, board);
  Range& r = ranges_[seat];
  double agg = stats_.rate(id, "aggFreq");
  double foldVsBet = stats_.rate(id, "foldToBet");
  double bluffW = clampd(0.06 + agg * 0.55, 0.06, 0.6);
  double stationW = clampd(0.62 - foldVsBet, 0.04, 0.5);
  bool big = static_cast<double>(e.amount) / std::max(1, e.potBefore) > 0.9 || e.allIn;
  float max = 0;
  for (int k = 0; k < NUM_COMBOS; ++k) {
    double st = s[k];
    if (st < 0) {
      r[k] = 0;
      continue;
    }
    double L = 1;
    if (e.type == Act::Bet) L = st >= 0.68 ? 1 : st >= 0.45 ? (big ? 0.18 : 0.38) : bluffW * (big ? 0.8 : 1);
    else if (e.type == Act::Raise) L = st >= 0.8 ? 1 : st >= 0.6 ? 0.3 : st >= 0.45 ? 0.12 : bluffW * 0.55;
    else if (e.type == Act::Call) {
      L = st >= 0.92 ? 0.5 : st >= 0.35 ? 1 : st >= 0.2 ? 0.25 + stationW * 0.5 : stationW;
      if (e.allIn || big) L *= st >= 0.5 ? 1 : 0.4;
    } else if (e.type == Act::Check)
      L = st >= 0.88 ? clampd(0.75 - agg, 0.25, 0.6) : 1;
    r[k] = static_cast<float>(r[k] * L);
    max = std::max(max, r[k]);
  }
  normalize(seat, max);
}

}  // namespace hl
