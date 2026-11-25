#include "core/equity.hpp"

#include <algorithm>

#include "core/evaluator.hpp"

namespace hl {

namespace {
struct Sampler {
  std::vector<short> idx;
  std::vector<double> cum;
  double total = 0;
};

Sampler buildSampler(const Range& w, const bool* dead) {
  Sampler s;
  const auto& D = R();
  for (int k = 0; k < NUM_COMBOS; ++k) {
    if (w[k] <= 0 || dead[D.c1[k]] || dead[D.c2[k]]) continue;
    s.total += w[k];
    s.idx.push_back(static_cast<short>(k));
    s.cum.push_back(s.total);
  }
  return s;
}

int sampleCombo(const Sampler& s, Rng& rng) {
  double x = rng.next() * s.total;
  auto it = std::upper_bound(s.cum.begin(), s.cum.end(), x);
  size_t i = std::min(static_cast<size_t>(it - s.cum.begin()), s.idx.size() - 1);
  return s.idx[i];
}
}  // namespace

EquityResult calcEquity(const std::array<Card, 2>& hero, const std::vector<Card>& board, const std::vector<Opponent>& opps, int iterations, Rng& rng) {
  bool dead[52] = {};
  dead[hero[0]] = dead[hero[1]] = true;
  for (Card c : board) dead[c] = true;
  for (auto& o : opps)
    if (o.known) dead[o.cards[0]] = dead[o.cards[1]] = true;
  const int nOpp = static_cast<int>(opps.size());
  std::vector<Sampler> samplers(nOpp);
  std::vector<bool> useSampler(nOpp, false);
  for (int i = 0; i < nOpp; ++i)
    if (!opps[i].known && opps[i].range) {
      samplers[i] = buildSampler(*opps[i].range, dead);
      useSampler[i] = samplers[i].total > 0;
    }
  const int nb = static_cast<int>(board.size());
  const int need = 5 - nb;
  Card heroH[7];
  heroH[0] = hero[0];
  heroH[1] = hero[1];
  for (int i = 0; i < nb; ++i) heroH[2 + i] = board[i];
  std::vector<std::array<Card, 7>> oh(nOpp);
  for (auto& h : oh)
    for (int i = 0; i < nb; ++i) h[2 + i] = board[i];

  double share = 0;
  int wins = 0, ties = 0, samples = 0;
  bool used[52];
  for (int it = 0; it < iterations * 3 && samples < iterations; ++it) {
    std::copy(dead, dead + 52, used);
    bool ok = true;
    for (int o = 0; o < nOpp && ok; ++o) {
      if (opps[o].known) {
        oh[o][0] = opps[o].cards[0];
        oh[o][1] = opps[o].cards[1];
        continue;
      }
      bool got = false;
      for (int t = 0; t < 25 && !got; ++t) {
        int a, b;
        if (useSampler[o]) {
          int k = sampleCombo(samplers[o], rng);
          a = R().c1[k];
          b = R().c2[k];
        } else {
          a = rng.below(52);
          b = rng.below(52);
          if (a == b) continue;
        }
        if (used[a] || used[b]) continue;
        used[a] = used[b] = true;
        oh[o][0] = a;
        oh[o][1] = b;
        got = true;
      }
      ok = got;
    }
    if (!ok) continue;
    for (int k = 0; k < need; ++k) {
      int c;
      do c = rng.below(52);
      while (used[c]);
      used[c] = true;
      heroH[2 + nb + k] = c;
      for (auto& h : oh) h[2 + nb + k] = c;
    }
    int hs = evaluate(heroH, 7);
    int best = -1, bestCount = 0;
    for (auto& h : oh) {
      int s = evaluate(h.data(), 7);
      if (s > best) {
        best = s;
        bestCount = 1;
      } else if (s == best)
        ++bestCount;
    }
    ++samples;
    if (hs > best) {
      ++wins;
      share += 1;
    } else if (hs == best) {
      ++ties;
      share += 1.0 / (bestCount + 1);
    }
  }
  EquityResult r;
  if (samples == 0) return r;
  r.samples = samples;
  r.equity = share / samples;
  r.win = static_cast<double>(wins) / samples;
  r.tie = static_cast<double>(ties) / samples;
  return r;
}

std::vector<double> knownEquity(const std::vector<std::array<Card, 2>>& hands, const std::vector<Card>& board, int iterations, Rng& rng) {
  const int n = static_cast<int>(hands.size());
  const int nb = static_cast<int>(board.size());
  const int need = 5 - nb;
  bool dead[52] = {};
  for (auto& h : hands) dead[h[0]] = dead[h[1]] = true;
  for (Card c : board) dead[c] = true;
  std::vector<Card> live;
  for (int c = 0; c < 52; ++c)
    if (!dead[c]) live.push_back(c);
  std::vector<std::array<Card, 7>> bufs(n);
  for (int i = 0; i < n; ++i) {
    bufs[i][0] = hands[i][0];
    bufs[i][1] = hands[i][1];
    for (int j = 0; j < nb; ++j) bufs[i][2 + j] = board[j];
  }
  std::vector<double> eq(n, 0.0);
  std::vector<int> scores(n);
  long count = 0;
  auto settle = [&]() {
    int best = -1, k = 0;
    for (int i = 0; i < n; ++i) {
      scores[i] = evaluate(bufs[i].data(), 7);
      if (scores[i] > best) {
        best = scores[i];
        k = 1;
      } else if (scores[i] == best)
        ++k;
    }
    for (int i = 0; i < n; ++i)
      if (scores[i] == best) eq[i] += 1.0 / k;
    ++count;
  };
  auto setCard = [&](int pos, Card c) {
    for (auto& b : bufs) b[pos] = c;
  };
  const int base = 2 + nb;
  if (need == 0)
    settle();
  else if (need == 1) {
    for (Card c : live) {
      setCard(base, c);
      settle();
    }
  } else if (need == 2) {
    for (size_t i = 0; i < live.size(); ++i) {
      setCard(base, live[i]);
      for (size_t j = i + 1; j < live.size(); ++j) {
        setCard(base + 1, live[j]);
        settle();
      }
    }
  } else {
    std::vector<Card> pool = live;
    for (int it = 0; it < iterations; ++it) {
      for (int k = 0; k < need; ++k) {
        int j = k + rng.below(static_cast<int>(pool.size()) - k);
        std::swap(pool[k], pool[j]);
        setCard(base + k, pool[k]);
      }
      settle();
    }
  }
  for (auto& v : eq) v /= static_cast<double>(count);
  return eq;
}

}  // namespace hl
