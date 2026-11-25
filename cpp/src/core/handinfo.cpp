#include "core/handinfo.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "core/evaluator.hpp"

namespace hl {

static int rankMask(const std::vector<Card>& cs) {
  int m = 0;
  for (Card c : cs) m |= 1 << (c >> 2);
  return m;
}

static std::vector<Card> concat(const std::vector<Card>& a, const std::vector<Card>& b) {
  std::vector<Card> o = a;
  o.insert(o.end(), b.begin(), b.end());
  return o;
}

std::string DrawInfo::text() const {
  std::string s;
  for (size_t i = 0; i < description.size(); ++i) s += (i ? ", " : "") + description[i];
  return s;
}

DrawInfo drawInfo(const std::vector<Card>& hole, const std::vector<Card>& board) {
  DrawInfo r;
  if (board.size() < 3 || board.size() > 4) return r;
  auto all = concat(hole, board);
  int cur = evaluate(all);
  const auto& T = evalTables();
  for (int s = 0; s < 4; ++s) {
    int hs = 0, bs = 0;
    std::set<int> boardRanks;
    bool holeTop = false;
    for (Card c : hole)
      if ((c & 3) == s) ++hs;
    for (Card c : board)
      if ((c & 3) == s) {
        ++bs;
        boardRanks.insert(c >> 2);
      }
    if (hs == 0) continue;
    if (hs + bs == 4 && categoryOf(cur) < FLUSH) {
      r.flushDraw = true;
      int top = 12;
      while (boardRanks.count(top)) --top;
      for (Card c : hole)
        if ((c & 3) == s && (c >> 2) == top) holeTop = true;
      r.nutFlushDraw = holeTop;
    } else if (hs + bs == 3 && board.size() == 3)
      r.backdoorFlush = true;
  }
  int m = rankMask(all), bm = rankMask(board);
  if (T.straightHigh[m] < 0) {
    int outsRanks = 0;
    for (int rr = 0; rr < 13; ++rr) {
      if (m & (1 << rr)) continue;
      if (T.straightHigh[m | (1 << rr)] >= 0 && T.straightHigh[bm | (1 << rr)] < 0) ++outsRanks;
    }
    if (outsRanks >= 2) r.oesd = true;
    else if (outsRanks == 1) r.gutshot = true;
  }
  if (r.flushDraw) r.description.push_back(r.nutFlushDraw ? "nut flush draw" : "flush draw");
  if (r.oesd) r.description.push_back("open-ended straight draw");
  else if (r.gutshot) r.description.push_back("gutshot");
  if (r.backdoorFlush && !r.flushDraw) r.description.push_back("backdoor flush draw");
  return r;
}

static int boardOnlyCategory(const std::vector<Card>& cards) {
  std::map<int, int> counts;
  for (Card c : cards) counts[c >> 2]++;
  std::vector<int> v;
  for (auto& kv : counts) v.push_back(kv.second);
  std::sort(v.rbegin(), v.rend());
  v.push_back(0);
  if (v[0] >= 4) return QUADS;
  if (v[0] == 3 && v[1] >= 2) return FULL_HOUSE;
  if (v[0] == 3) return TRIPS;
  if (v[0] == 2 && v[1] == 2) return TWO_PAIR;
  if (v[0] == 2) return PAIR;
  return HIGH_CARD;
}

Outs countOuts(const std::vector<Card>& hole, const std::vector<Card>& board, const std::vector<Card>& extraDead) {
  Outs o;
  if (board.size() < 3 || board.size() > 4) return o;
  auto all = concat(hole, board);
  bool used[52] = {};
  for (Card c : all) used[c] = true;
  for (Card c : extraDead) used[c] = true;
  int curCat = categoryOf(evaluate(all));
  for (int c = 0; c < 52; ++c) {
    if (used[c]) continue;
    auto next = all;
    next.push_back(c);
    int nc = categoryOf(evaluate(next));
    auto bn = board;
    bn.push_back(c);
    int boardCat = bn.size() >= 5 ? categoryOf(evaluate(bn)) : boardOnlyCategory(bn);
    if (nc > curCat && nc > boardCat) {
      ++o.improveOuts;
      if (nc >= STRAIGHT && curCat < STRAIGHT) {
        ++o.drawOuts;
        o.cards.push_back(c);
      }
    }
  }
  return o;
}

Texture boardTexture(const std::vector<Card>& board) {
  Texture t;
  if (board.size() < 3) return t;
  t.valid = true;
  int suits[4] = {};
  for (Card c : board) suits[c & 3]++;
  int maxSuit = *std::max_element(suits, suits + 4);
  std::set<int> ranks;
  int high = 0;
  for (Card c : board) {
    ranks.insert(c >> 2);
    high = std::max(high, c >> 2);
  }
  t.paired = ranks.size() < board.size();
  int m = rankMask(board);
  int windows = 0;
  for (int lo = -1; lo <= 8; ++lo) {
    int cnt = 0;
    for (int r = lo; r < lo + 5; ++r) {
      int rr = r == -1 ? 12 : r;
      if (m & (1 << rr)) ++cnt;
    }
    if (cnt >= 3) ++windows;
  }
  t.monotone = maxSuit >= 3;
  t.twoTone = maxSuit == 2;
  t.rainbow = maxSuit == 1;
  t.connected = windows >= 2;
  double wet = t.monotone ? 0.45 : t.twoTone ? 0.25 : 0;
  wet += std::min(0.45, windows * 0.12);
  if (t.paired) wet -= 0.1;
  t.wetness = std::clamp(wet, 0.0, 1.0);
  std::vector<std::string> labels;
  if (maxSuit == static_cast<int>(board.size())) labels.push_back("monotone");
  else if (maxSuit >= 4) labels.push_back("four-flush");
  else if (maxSuit == 3) labels.push_back(board.size() == 3 ? "monotone" : "flush possible");
  else labels.push_back(t.twoTone ? "two-tone" : "rainbow");
  if (t.paired) labels.push_back("paired");
  if (t.connected) labels.push_back("connected");
  labels.push_back(high >= 10 ? std::string(rankName(high)) + "-high" : "low");
  for (size_t i = 0; i < labels.size(); ++i) t.description += (i ? ", " : "") + labels[i];
  return t;
}

HandAnalysis analyzeHand(const std::vector<Card>& hole, const std::vector<Card>& board) {
  HandAnalysis a;
  auto all = concat(hole, board);
  std::vector<int> h = {hole[0] >> 2, hole[1] >> 2};
  std::sort(h.rbegin(), h.rend());
  if (board.empty()) {
    a.category = h[0] == h[1] ? PAIR : HIGH_CARD;
    bool suited = (hole[0] & 3) == (hole[1] & 3);
    a.name = h[0] == h[1] ? std::string("Pocket ") + rankPlural(h[0]) : std::string(rankName(h[0])) + "-" + rankName(h[1]) + (suited ? " suited" : " offsuit");
    a.detail = h[0] == h[1] ? "pocket pair" : "";
    return a;
  }
  a.score = evaluate(all);
  a.category = categoryOf(a.score);
  a.name = describeScore(a.score);
  a.draws = drawInfo(hole, board);
  a.outs = countOuts(hole, board);
  a.texture = boardTexture(board);
  std::vector<int> br;
  for (Card c : board) br.push_back(c >> 2);
  std::sort(br.rbegin(), br.rend());
  br.erase(std::unique(br.begin(), br.end()), br.end());
  if (board.size() >= 5 && evaluate(board) == a.score) {
    a.detail = "playing the board";
    return a;
  }
  auto has = [&](int r) { return std::find(br.begin(), br.end(), r) != br.end(); };
  if (a.category == PAIR) {
    if (h[0] == h[1]) {
      if (h[0] > br.front()) a.detail = "overpair";
      else if (h[0] < br.back()) a.detail = "underpair";
      else a.detail = "pocket pair below top card";
    } else {
      int pr = -1;
      for (int r : br)
        if (r == h[0] || r == h[1]) {
          pr = r;
          break;
        }
      if (pr < 0) a.detail = "pair on board";
      else {
        int kicker = h[0] == pr ? h[1] : h[0];
        auto idx = std::find(br.begin(), br.end(), pr) - br.begin();
        if (idx == 0) {
          int best = 12;
          while (has(best)) --best;
          a.detail = kicker == best ? "top pair, top kicker" : kicker >= 9 ? "top pair, good kicker" : "top pair, weak kicker";
        } else if (idx == static_cast<long>(br.size()) - 1)
          a.detail = "bottom pair";
        else
          a.detail = "middle pair";
      }
    }
  } else if (a.category == TWO_PAIR) {
    a.detail = (h[0] != h[1] && has(h[0]) && has(h[1])) ? "two pair (both cards)" : "two pair";
  } else if (a.category == TRIPS) {
    a.detail = h[0] == h[1] ? "set" : "trips";
  } else if (a.category == HIGH_CARD) {
    int overs = (h[0] > br.front()) + (h[1] > br.front());
    a.detail = overs == 2 ? "two overcards" : overs == 1 ? "one overcard" : "no pair";
  }
  return a;
}

double outsToEquity(int outs, int cardsToCome, int unseen) {
  if (cardsToCome <= 0 || outs <= 0) return 0;
  if (cardsToCome == 1) return static_cast<double>(outs) / unseen;
  double miss = (static_cast<double>(unseen - outs) / unseen) * (static_cast<double>(unseen - 1 - outs) / (unseen - 1));
  return 1 - miss;
}

}  // namespace hl
