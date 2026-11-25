#include "bots/strategy.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "core/equity.hpp"

namespace hl {

namespace {
double clampd(double x, double a, double b) { return x < a ? a : x > b ? b : x; }

std::string pctStr(double x, int digits = 0) {
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(digits);
  o << x * 100 << "%";
  return o.str();
}

const ClassSet& bluff3bet() {
  static const ClassSet s = parseRange("A5s-A2s, K9s-K6s, Q9s, J9s, T9s, T8s, 98s, 97s, 87s, 86s, 76s, 75s, 65s, 54s");
  return s;
}
const ClassSet& speculative() {
  static const ClassSet s = parseRange("22-99, A2s-A9s, KTs-K9s, QTs, Q9s, JTs, J9s, T9s, T8s, 98s, 87s, 76s, 65s, 54s");
  return s;
}

double shoveBase(const std::string& pos, int n) {
  static const std::map<std::string, double> m = {{"UTG", 0.12}, {"UTG+1", 0.13}, {"UTG+2", 0.14}, {"LJ", 0.16}, {"HJ", 0.19},
                                                  {"CO", 0.25},  {"BTN", 0.36},   {"SB", 0.48},    {"BB", 0.5}};
  if (n == 2) return 0.62;
  auto it = m.find(pos);
  return it == m.end() ? 0.2 : it->second;
}

int orderKey(const ViewPlayer& p, int n) { return p.orderFromButton == 0 ? n : p.orderFromButton; }

int sizeTo(const View& v, double to) {
  int t = static_cast<int>(std::lround(to));
  if (t >= (v.me.stack + v.me.bet) * 0.42) t = v.legal.maxTo;  // committing: just shove
  return std::max(v.legal.minTo, std::min(v.legal.maxTo, t));
}

Option opt(Act a, int amount, double w) {
  Option o;
  o.type = a;
  o.amount = amount;
  o.w = w;
  return o;
}
}  // namespace

std::vector<const ViewPlayer*> liveOpponents(const View& v) {
  std::vector<const ViewPlayer*> out;
  for (auto& p : v.players)
    if (p.seat != v.me.seat && !p.folded) out.push_back(&p);
  return out;
}

bool isInPosition(const View& v, int vsSeat) {
  int meKey = orderKey(v.me, v.numPlayers);
  for (auto* o : liveOpponents(v))
    if ((vsSeat < 0 || o->seat == vsSeat) && orderKey(*o, v.numPlayers) > meKey) return false;
  return true;
}

int effectiveStack(const View& v) {
  int mine = v.me.stack + v.me.bet, other = 0;
  for (auto* o : liveOpponents(v)) other = std::max(other, o->stack + o->bet);
  return std::min(mine, other);
}

PreflopCtx preflopContext(const View& v) {
  PreflopCtx c;
  c.lastRaiseTo = v.bb;
  for (auto& e : v.log) {
    if (e.street != Street::Preflop || e.blind || e.type == Act::Uncalled) continue;
    if (e.type == Act::Raise) {
      ++c.raises;
      c.lastRaiserSeat = e.seat;
      c.lastRaiseTo = e.to;
      c.callersAfterRaise = 0;
      if (c.raises == 1) c.openerSeat = e.seat;
      if (c.raises == 2) c.threeBettorSeat = e.seat;
      if (e.seat == v.me.seat) c.iRaised = true;
    } else if (e.type == Act::Call) {
      if (c.raises == 0) ++c.limpers;
      else ++c.callersAfterRaise;
    }
  }
  return c;
}

std::vector<Option> chooseWeights(std::vector<Option> options, const Profile& prof, double pot, double tilt) {
  double skill = clampd(prof.skill * (1 - tilt * 0.4), 0.05, 1);
  double tau = (0.012 + (1 - skill) * 0.2) * (1 + tilt);
  auto cat = [](const Option& o) { return isAggressive(o.type) ? 3 : static_cast<int>(o.type); };
  std::vector<double> u(options.size());
  for (size_t i = 0; i < options.size(); ++i) {
    const Option& o = options[i];
    double x = o.ev / std::max(1.0, pot);
    if (o.type == Act::Call) x += prof.callBias;
    if (o.type == Act::Fold) x += prof.foldBias;
    if (isAggressive(o.type)) x += prof.aggression + tilt * 0.08;
    u[i] = x;
  }
  std::map<int, double> catU;
  for (size_t i = 0; i < options.size(); ++i) {
    int c = cat(options[i]);
    auto it = catU.find(c);
    if (it == catU.end() || u[i] > it->second) catU[c] = u[i];
  }
  double m = -1e18;
  for (auto& kv : catU) m = std::max(m, kv.second);
  std::map<int, double> catEx;
  double catSum = 0;
  for (auto& kv : catU) catSum += (catEx[kv.first] = std::exp((kv.second - m) / tau));
  double sizeTau = tau * 0.6;
  std::map<int, double> inner;
  for (size_t i = 0; i < options.size(); ++i) inner[cat(options[i])] += std::exp((u[i] - catU[cat(options[i])]) / sizeTau);
  for (size_t i = 0; i < options.size(); ++i) {
    int c = cat(options[i]);
    options[i].w = (catEx[c] / catSum) * (std::exp((u[i] - catU[c]) / sizeTau) / inner[c]);
  }
  return options;
}

double realization(Street s, bool ip, int nOpp) {
  if (s == Street::River) return 1;
  double base = s == Street::Flop ? (ip ? 0.97 : 0.86) : (ip ? 0.99 : 0.92);
  return clampd(base - 0.04 * (nOpp - 1), 0.6, 1);
}

static PreflopResult allInDecision(const View& v, RangeTracker* tracker, const Profile& prof, Rng& rng, int iterations, PreflopResult res) {
  const double P = v.potTotal;
  auto opps = liveOpponents(v);
  std::vector<Opponent> os;
  for (auto* o : opps) {
    Opponent x;
    if (tracker) x.range = &tracker->range(o->seat);
    os.push_back(x);
  }
  double eq = calcEquity(v.hole, {}, os, iterations, rng).equity;
  int callAmt = v.legal.toCall;
  double real = callAmt >= v.me.stack ? 1 : 0.9;
  std::vector<Option> options;
  Option f = opt(Act::Fold, 0, 0);
  f.hasEv = true;
  options.push_back(f);
  Option c = opt(Act::Call, callAmt, 0);
  c.ev = eq * real * (P + callAmt) - callAmt;
  c.hasEv = true;
  options.push_back(c);
  if (v.legal.canRaise) {
    int to = v.legal.maxTo;
    int add = to - v.me.bet;
    const PreflopCtx& ctx = res.meta.ctx;
    double contFrac = ctx.raises >= 3 ? 0.7 : ctx.raises == 2 ? 0.5 : 0.4;
    double foldAll = 1, potCalled = P + add;
    std::vector<Range> truncated;
    truncated.reserve(opps.size());
    std::vector<Opponent> called;
    for (auto* o : opps) {
      Opponent x;
      if (o->allIn) {
        foldAll = 0;
        if (tracker) x.range = &tracker->range(o->seat);
        called.push_back(x);
        continue;
      }
      double fo = o->seat == ctx.lastRaiserSeat ? 1 - contFrac : 0.92;
      foldAll *= fo;
      if (tracker) {
        Strengths s = tracker->strengths(o->seat, {}, {v.hole[0], v.hole[1]});
        truncated.push_back(truncateTop(tracker->range(o->seat), s, 1 - fo));
      }
      called.push_back(x);
      potCalled += (1 - fo) * (std::min(o->stack + o->bet, to) - o->bet);
    }
    size_t ti = 0;
    for (size_t i = 0; i < opps.size() && tracker; ++i)
      if (!opps[i]->allIn) called[i].range = &truncated[ti++];
    double eqC = calcEquity(v.hole, {}, called, static_cast<int>(iterations * 0.7), rng).equity;
    Option r = opt(Act::Raise, to, 0);
    r.ev = foldAll * P + (1 - foldAll) * (eqC * potCalled - add);
    r.hasEv = true;
    options.push_back(r);
  }
  double needed = static_cast<double>(callAmt) / (P + callAmt);
  res.meta.equity = eq;
  res.meta.needed = needed;
  res.notes.push_back("Big decision: " + res.meta.hand + " has ~" + pctStr(eq) + " equity vs the estimated range(s); calling needs " + pctStr(needed) + ".");
  res.options = chooseWeights(options, prof, P, 0);
  res.evBased = true;
  return res;
}

PreflopResult preflopStrategy(const View& v, RangeTracker* tracker, const Profile& prof, Rng& rng, int iterations, double tilt) {
  const auto& D = R();
  const Legal& legal = v.legal;
  const int bb = v.bb;
  int cls = classOf(v.hole[0], v.hole[1]);
  const HandClass& hc = D.classes[cls];
  double pct = D.percentile[cls];
  PreflopResult res;
  PreflopCtx ctx = preflopContext(v);
  const int n = v.numPlayers;
  double loose = prof.looseness * (1 + tilt * 0.5);
  double effBB = static_cast<double>(effectiveStack(v)) / bb;
  double P = v.potTotal;
  res.meta.hand = hc.name;
  res.meta.pct = pct;
  res.meta.position = v.me.position;
  res.meta.effBB = effBB;
  res.meta.ctx = ctx;
  bool canRaise = legal.canRaise;
  auto raiseOpt = [&](double to, double w) { return opt(Act::Raise, sizeTo(v, to), w); };

  int myTotal = v.me.stack + v.me.bet;
  bool committing = legal.toCall > 0 && (legal.toCall >= myTotal * 0.3 || (ctx.raises >= 1 && effBB <= 16) || ctx.raises >= 3);
  if (committing) return allInDecision(v, tracker, prof, rng, iterations, res);

  if (ctx.raises == 0) {
    if (v.me.position == "BB" && legal.toCall == 0) {
      double w = clampd(0.1 * loose * (1 + prof.aggression * 3), 0.03, 0.4);
      double pr = canRaise ? softIn(pct, w) : 0;
      res.meta.width = w;
      res.notes.push_back("Option in the big blind: raise the top ~" + pctStr(w) + " vs limpers, otherwise check.");
      res.options = {raiseOpt(bb * (3.5 + ctx.limpers), pr), opt(Act::Check, 0, 1 - pr)};
      return res;
    }
    std::string key = rfiKey(v.me.position, n);
    if (key.empty()) key = v.me.position;
    if (effBB <= 13) {
      double w = clampd(shoveBase(v.me.position, n) * std::pow(10.0 / std::max(effBB, 1.0), 0.7) * loose, 0.04, 0.95);
      if (ctx.limpers) w *= 0.7;
      double ps = softIn(pct, w);
      res.meta.width = w;
      std::ostringstream o;
      o.setf(std::ios::fixed);
      o.precision(1);
      o << "Short stack (" << effBB << "bb): push/fold. Shove the top ~" << pctStr(w) << " from " << v.me.position << ".";
      res.notes.push_back(o.str());
      res.options = {opt(Act::Raise, legal.maxTo, ps), opt(Act::Fold, 0, 1 - ps)};
      return res;
    }
    const ClassSet* set = rfiSet(key);
    double width = set ? rfiWidth(key) : 0.2;
    bool inChart = set ? set->test(cls) : pct <= width;
    double pRaise, w = width;
    if (ctx.limpers > 0) {
      w = width * 0.75 * loose;
      pRaise = softIn(pct, w);
      res.notes.push_back(std::to_string(ctx.limpers) + " limper(s): isolate with a tighter range (~" + pctStr(w) + ").");
    } else if (loose <= 1)
      pRaise = inChart ? softIn(pct, std::max(width * loose * 1.25, 0.02)) : 0;
    else
      pRaise = inChart ? 1 : softIn(pct, width * loose);
    if (prof.pfrMult > 0 && prof.pfrMult < 1) pRaise *= softIn(pct, width * prof.pfrMult * 1.4);
    res.meta.width = w;
    res.meta.inChart = inChart;
    if (ctx.limpers == 0)
      res.notes.push_back(hc.name + " is " + (inChart ? "" : "not ") + "in the standard " + (key == "HU" ? std::string("heads-up button") : v.me.position) +
                          " opening range (~" + pctStr(width) + " of hands).");
    double pLimp = 0;
    if (prof.limp > 0) {
      double lw = clampd(width * loose * 1.7, 0, 0.95);
      pLimp = (1 - pRaise) * softIn(pct, lw) * prof.limp;
    } else if (ctx.limpers > 0 && speculative().test(cls) && isInPosition(v))
      pLimp = (1 - pRaise) * 0.6;
    else if (v.me.position == "SB" && n > 2 && speculative().test(cls) && prof.skill < 0.9)
      pLimp = (1 - pRaise) * 0.3;
    double openBB = prof.openSize > 0 ? prof.openSize : (v.me.position == "SB" ? 3 : n == 2 ? 2.5 : 2.3);
    res.options = {raiseOpt(bb * (openBB + ctx.limpers), canRaise ? pRaise : 0), opt(Act::Call, legal.toCall, pLimp + (canRaise ? 0 : pRaise)),
                   opt(Act::Fold, 0, std::max(0.0, 1 - pRaise - pLimp))};
    return res;
  }

  if (ctx.raises == 1) {
    const ViewPlayer* opener = v.player(ctx.openerSeat);
    double w = tracker ? clampd(tracker->width(ctx.openerSeat), 0.02, 1) : rfiWidth(opener->position);
    bool ip = isInPosition(v, ctx.openerSeat);
    double price = legal.toCall / (P + legal.toCall);
    bool isBB = v.me.position == "BB", isSB = v.me.position == "SB";
    double v3 = clampd(w * 0.22, 0.028, 0.11) * prof.threeBetMult * (1 + tilt * 0.4);
    double cont;
    if (isBB) cont = w * (1 + 1.3 * (1 - w) * (1 - w)) * clampd(std::pow(0.27 / price, 0.6), 0.5, 1.6);
    else if (isSB) cont = w * 0.55;
    else cont = w * (ip ? 0.65 : 0.5);
    cont *= std::pow(0.85, ctx.callersAfterRaise);
    if (effBB < 25) cont *= 0.7;
    cont = clampd(cont * loose * (1 + prof.callBias * 4), 0.01, 0.95);
    double pVal = softIn(pct, v3);
    double pBluff = 0;
    if (bluff3bet().test(cls) && pct > v3) pBluff = prof.bluff3bet * ((ip || isBB || isSB) ? 1 : 0.5) * (w > 0.2 ? 1 : 0.35);
    double p3 = canRaise ? pVal + (1 - pVal) * pBluff : 0;
    double pCall = (1 - p3) * softIn(pct, cont);
    if (isSB && prof.skill >= 0.7) {
      double shift = pCall * 0.6 * (pct < cont * 0.6 ? 1 : 0.3);
      pCall -= shift;
      if (canRaise) p3 += shift;
    }
    res.notes.push_back(opener->position + " opened; estimated range ~" + pctStr(w) + " of hands.");
    res.notes.push_back("3-bet for value with the top ~" + pctStr(v3, 1) + ", continue with ~" + pctStr(cont) + (isBB ? " (the big blind gets a discount)." : "."));
    if (pBluff > 0) res.notes.push_back(hc.name + " has good blockers/playability for an occasional 3-bet bluff.");
    double mult = ip ? 3 : 3.8;
    double to = ctx.lastRaiseTo * mult + ctx.callersAfterRaise * ctx.lastRaiseTo;
    res.options = {raiseOpt(effBB < 25 ? legal.maxTo : to, p3), opt(Act::Call, legal.toCall, pCall), opt(Act::Fold, 0, std::max(0.0, 1 - p3 - pCall))};
    return res;
  }

  // facing a 3-bet, not yet stack-committing
  int tbSeat = ctx.lastRaiserSeat;
  double w3 = tracker ? clampd(tracker->width(tbSeat), 0.01, 1) : 0.08;
  bool ip = isInPosition(v, tbSeat);
  double myOpenW = (ctx.iRaised && tracker) ? tracker->width(v.me.seat) : 0.1;
  double v4 = clampd(w3 * 0.28, 0.018, 0.05) * prof.threeBetMult;
  double callW = ctx.iRaised ? std::max(w3 * (ip ? 1.5 : 1.1), myOpenW * (ip ? 0.35 : 0.28)) : clampd(w3 * 0.6, 0.015, 0.06);
  double pVal = softIn(pct, v4);
  double pBluff = (canRaise && (hc.name == "A5s" || hc.name == "A4s" || hc.name == "A3s")) ? prof.bluff3bet * 0.5 : 0;
  double p4 = canRaise ? pVal + (1 - pVal) * pBluff : 0;
  double pCall = (1 - p4) * softIn(pct, clampd(callW * loose * (1 + prof.callBias * 3), 0.01, 0.6));
  res.notes.push_back("Facing a 3-bet from a ~" + pctStr(w3) + " range: 4-bet the top ~" + pctStr(v4, 1) + ", call ~" + pctStr(callW) + ".");
  double to = ctx.lastRaiseTo * (ip ? 2.3 : 2.6);
  res.options = {raiseOpt(to, p4), opt(Act::Call, legal.toCall, pCall), opt(Act::Fold, 0, std::max(0.0, 1 - p4 - pCall))};
  return res;
}

Option evalAmount(const PostflopResult& c, int toRaw, Rng& rng) {
  const View& v = c.view;
  const Legal& legal = v.legal;
  const double P = v.potTotal;
  int to = std::min({toRaw, legal.maxTo, c.maxReach});
  to = std::max(to, std::min(legal.minTo, legal.maxTo));
  int add = to - v.me.bet;
  double foldAll = 1, callerAdd = 0, callerProb = 0;
  bool anyAllIn = false;
  std::vector<Range> truncated(c.oppSeats.size());
  std::vector<Opponent> called;
  for (size_t i = 0; i < c.oppSeats.size(); ++i) {
    const ViewPlayer* o = v.player(c.oppSeats[i]);
    Opponent x;
    if (o->allIn) {
      anyAllIn = true;
      x.range = &c.ranges[i];
      called.push_back(x);
      continue;
    }
    int facing = to - o->bet;
    double f = facing / (P + facing);
    double observed = c.stats ? c.stats->rate(o->id, "foldToBet") / 0.42 : 1.0;
    f *= 1 + (clampd(observed, 0.3, 2.2) - 1) * c.adapt;
    if (v.streetAggressor[streetIndex(v.street)] == o->seat) f *= 0.8;
    if (v.street == Street::River) f *= 1.08;
    // Even vs huge overbets players keep the top of their range; the cap rises only slowly with size.
    f = clampd(f, 0.02, clampd(0.86 + 0.03 * (facing / std::max(1.0, P)), 0.9, 0.97));
    foldAll *= f;
    truncated[i] = truncateTop(c.ranges[i], c.strengths[i], 1 - f);
    x.range = &truncated[i];
    called.push_back(x);
    callerAdd += (1 - f) * std::min(facing, o->stack);
    callerProb += 1 - f;
  }
  if (anyAllIn) foldAll = 0;
  double eqCalled = calcEquity(v.hole, v.board, called, static_cast<int>(c.iterations * 0.6), rng).equity;
  double eqC = clampd(eqCalled + c.percNoise, 0, 1);
  double condCallers = foldAll < 1 ? std::max(1.0, callerProb / (1 - foldAll)) : 1;
  double avgCallerAdd = callerProb > 0 ? callerAdd / callerProb : 0;
  double potCalled = P + add + condCallers * avgCallerAdd;
  double r = (to >= legal.maxTo || v.street == Street::River) ? 1 : std::min(1.0, c.real + 0.05);
  bool canBeRaised = false;
  if (to < legal.maxTo)
    for (int s : c.oppSeats) {
      const ViewPlayer* o = v.player(s);
      if (!o->allIn && o->stack > to - o->bet) canBeRaised = true;
    }
  double raiseRisk = (canBeRaised && eqC < 0.55) ? 0.14 * (1 - foldAll) * (v.street == Street::River ? 0.7 : 1) : 0;
  double showdownPart = eqC * r * potCalled - add;
  Option o;
  o.type = legal.canBet ? Act::Bet : Act::Raise;
  o.amount = to;
  o.ev = foldAll * P + (1 - foldAll) * ((1 - raiseRisk) * showdownPart + raiseRisk * -add);
  o.hasEv = true;
  o.foldAll = foldAll;
  o.eqCalled = eqC;
  return o;
}

PostflopResult postflopOptions(const View& v, RangeTracker& tracker, const Profile& prof, Rng& rng, int iterations, double tilt) {
  PostflopResult res;
  res.view = v;
  res.iterations = iterations;
  res.stats = &tracker.stats();
  const Legal& legal = v.legal;
  const double P = v.potTotal;
  auto opps = liveOpponents(v);
  res.ip = isInPosition(v);
  std::vector<Card> dead = {v.hole[0], v.hole[1]};
  for (auto* o : opps) {
    res.oppSeats.push_back(o->seat);
    res.ranges.push_back(tracker.range(o->seat));
    res.strengths.push_back(tracker.strengths(o->seat, v.board, dead));
    res.maxReach = std::max(res.maxReach, o->stack + o->bet);
  }
  double skill = prof.skill;
  double noiseScale = (1 - skill) * 0.07 + tilt * 0.03;
  if (noiseScale > 0.001) {
    double u = 0;
    for (int i = 0; i < 4; ++i) u += rng.next();
    res.percNoise = (u - 2) * 0.866 * noiseScale;
  }
  std::vector<Opponent> os;
  for (auto& r : res.ranges) {
    Opponent o;
    o.range = &r;
    os.push_back(o);
  }
  res.equity = calcEquity(v.hole, v.board, os, iterations, rng).equity;
  res.perceived = clampd(res.equity + res.percNoise + (res.equity > 0.35 ? prof.overvalue : 0), 0, 1);
  res.real = realization(v.street, res.ip, static_cast<int>(opps.size()));
  res.numOpp = static_cast<int>(opps.size());
  res.needed = legal.toCall > 0 ? legal.toCall / (P + legal.toCall) : 0;
  res.adapt = prof.adapt;

  if (legal.toCall == 0) {
    Option c;
    c.type = Act::Check;
    c.ev = res.perceived * res.real * P;
    c.hasEv = true;
    res.options.push_back(c);
  } else {
    Option f;
    f.type = Act::Fold;
    f.hasEv = true;
    res.options.push_back(f);
    Option c;
    c.type = Act::Call;
    c.amount = legal.toCall;
    double r = (legal.toCall >= v.me.stack || v.street == Street::River) ? 1 : res.real;
    c.ev = res.perceived * r * (P + legal.toCall) - legal.toCall;
    c.hasEv = true;
    res.options.push_back(c);
  }
  if (legal.canBet || legal.canRaise) {
    std::set<int> amounts;
    if (legal.canBet) {
      std::vector<double> fr = {0.33, 0.5, 0.75, 1.0};
      if (v.street == Street::River && skill >= 0.7) fr.push_back(1.5);
      for (double f : fr) amounts.insert(static_cast<int>(std::lround(v.me.bet + f * P)));
    } else {
      amounts.insert(static_cast<int>(std::lround(legal.currentBet * 2.5)));
      amounts.insert(static_cast<int>(std::lround(legal.currentBet + (P + legal.toCall))));
    }
    double spr = (v.me.stack + v.me.bet) / std::max(1.0, P);
    int shoveAdd = legal.maxTo - v.me.bet;
    if (spr < 3 || shoveAdd <= 2 * (P + legal.toCall)) amounts.insert(legal.maxTo);
    std::set<int> seen;
    for (int a : amounts) {
      Option o = evalAmount(res, a, rng);
      if (seen.count(o.amount)) continue;
      seen.insert(o.amount);
      res.options.push_back(o);
    }
  }
  return res;
}

}  // namespace hl
