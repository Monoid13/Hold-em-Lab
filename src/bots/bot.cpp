#include "bots/bot.hpp"

#include <algorithm>
#include <cmath>

namespace hl {

namespace {
double clampd(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
}

void BotMemory::afterHand(double netBB, bool badBeat, const Profile& prof) {
  tilt *= 0.88;
  double resist = prof.tiltResist;
  if (badBeat) tilt += 0.45 * (1 - resist);
  if (netBB < -25) tilt += std::min(0.4, (-netBB / 100) * (1 - resist));
  if (netBB > 30) tilt *= 0.7;
  tilt = clampd(tilt, 0, 1);
}

const Option& sampleOption(const std::vector<Option>& options, Rng& rng) {
  double total = 0;
  for (auto& o : options) total += std::max(0.0, o.w);
  if (total <= 0) return options.back();
  double x = rng.next() * total;
  for (auto& o : options) {
    x -= std::max(0.0, o.w);
    if (x <= 0) return o;
  }
  return options.back();
}

int humanRound(double amount, int bb, Rng& rng, double skill) {
  double a = amount * (1 + (rng.next() - 0.5) * 0.12 * (1.2 - skill));
  double unit = a < 10 * bb ? std::max(1.0, bb / 2.0) : a < 40 * bb ? bb : a < 150 * bb ? bb * 5 : bb * 10;
  a = std::round(a / unit) * unit;
  return std::max(1, static_cast<int>(std::lround(a)));
}

std::string randomChat(const std::string& kind, Rng& rng) {
  const auto& lines = chatLines(kind);
  if (lines.empty()) return "";
  return lines[rng.below(static_cast<int>(lines.size()))];
}

static int thinkTime(const View& v, const Action& a, double difficulty, double strength, const Profile& prof, Rng& rng) {
  double potBB = static_cast<double>(v.potTotal) / v.bb;
  double ms = v.street == Street::Preflop ? 450 : 800;
  ms += difficulty * 2200;
  ms += std::log1p(potBB) * 220;
  if (v.street == Street::River) ms += 500;
  if (a.type == Act::Fold && v.street == Street::Preflop && difficulty < 0.2) ms = 250 + rng.next() * 400;  // snap-fold trash
  if (isAggressive(a.type) && a.amount >= v.legal.maxTo) ms += 900;
  if (strength > 0.85 && rng.next() < prof.hollywood) ms += 1500 + rng.next() * 2000;  // Hollywood tank
  ms *= 0.7 + rng.next() * 0.7;
  return static_cast<int>(clampd(ms, 200, 9000));
}

Decision decideBot(const Table& t, int seat, const Profile& prof, BotMemory* mem, RangeTracker& tracker, Rng& rng, double iterationsScale) {
  View v = buildView(t, seat);
  const Legal& legal = v.legal;
  double tilt = mem ? mem->tilt : 0;
  int iterations = static_cast<int>((300 + prof.skill * 900) * iterationsScale);
  double P = v.potTotal;
  Option choice;
  double difficulty = 0.3, strength = 0.5;

  auto difficultyOf = [](const std::vector<Option>& opts) {
    std::vector<double> ws;
    for (auto& o : opts) ws.push_back(o.w);
    std::sort(ws.rbegin(), ws.rend());
    return 1 - (ws[0] - (ws.size() > 1 ? ws[1] : 0));
  };

  if (v.street == Street::Preflop) {
    PreflopResult s = preflopStrategy(v, &tracker, prof, rng, iterations, tilt);
    choice = sampleOption(s.options, rng);
    difficulty = difficultyOf(s.options);
    strength = 1 - s.meta.pct;
  } else {
    PostflopResult r = postflopOptions(v, tracker, prof, rng, iterations, tilt);
    strength = r.equity;
    bool chosen = false;
    if (legal.toCall == 0 && r.equity > 0.85 && rng.next() < prof.trap) {  // slow-play monsters sometimes
      for (auto& o : r.options)
        if (o.type == Act::Check) {
          choice = o;
          chosen = true;
        }
    }
    if (!chosen) {
      auto weighted = chooseWeights(r.options, prof, P, tilt);
      if (prof.sizingTell) {  // weak players size by hand strength, not EV
        std::vector<Option*> aggr;
        double aggrW = 0;
        for (auto& o : weighted)
          if (isAggressive(o.type)) {
            aggr.push_back(&o);
            aggrW += o.w;
          }
        if (aggr.size() > 1 && aggrW > 0) {
          std::sort(aggr.begin(), aggr.end(), [](Option* a, Option* b) { return a->amount < b->amount; });
          size_t n = aggr.size();
          Option* pick = r.equity > 0.72 ? aggr[std::min(n - 1, static_cast<size_t>(n * 0.75))] : r.equity < 0.4 ? aggr[0] : aggr[n / 3];
          for (auto* o : aggr) o->w = o == pick ? aggrW : 0;
        }
      }
      choice = sampleOption(weighted, rng);
      difficulty = difficultyOf(weighted);
    }
  }

  Action action{choice.type, 0};
  if (isAggressive(choice.type)) {
    int amt = choice.amount;
    if (amt < legal.maxTo) amt = humanRound(amt, v.bb, rng, prof.skill);
    amt = std::max(legal.minTo, std::min(legal.maxTo, amt));
    if (amt >= legal.maxTo * 0.93) amt = legal.maxTo;
    action = {legal.canBet ? Act::Bet : Act::Raise, amt};
    if (!legal.canBet && !legal.canRaise) action = {legal.toCall > 0 ? Act::Call : Act::Check, 0};
  } else if (choice.type == Act::Call)
    action = {legal.toCall > 0 ? Act::Call : Act::Check, 0};
  else if (choice.type == Act::Check && legal.toCall > 0)
    action = {Act::Fold, 0};
  else if (choice.type == Act::Fold && legal.toCall == 0)
    action = {Act::Check, 0};

  Decision d;
  d.action = action;
  d.thinkMs = thinkTime(v, action, difficulty, strength, prof, rng);
  if (tilt > 0.5 && isAggressive(action.type) && rng.next() < 0.15) d.chat = randomChat("tiltPlay", rng);
  return d;
}

}  // namespace hl
