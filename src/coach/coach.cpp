#include "coach/coach.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

#include "bots/profiles.hpp"
#include "core/equity.hpp"

namespace hl {

const char* gradeLabel(GradeLevel g) {
  static const char* l[] = {"Best", "Good", "Inaccuracy", "Mistake", "Blunder"};
  return l[static_cast<int>(g)];
}
const char* gradeSymbol(GradeLevel g) {
  static const char* s[] = {"*", "ok", "?!", "?", "??"};
  return s[static_cast<int>(g)];
}
double gradeScore(GradeLevel g) {
  static const double s[] = {1, 0.85, 0.5, 0.2, 0};
  return s[static_cast<int>(g)];
}

const std::vector<LeakTag>& leakTags() {
  static const std::vector<LeakTag> t = {
      {"preflop-too-loose", "Playing too many hands preflop", "Tighten up, especially out of position. The Ranges screen shows opening charts by position."},
      {"preflop-too-tight", "Folding playable hands preflop", "You pass up profitable opens and defends. Late position and the big blind can play much wider."},
      {"open-limp", "Open-limping", "Entering the pot with a call forfeits the initiative. Raise or fold when first in."},
      {"passive-preflop", "Flatting hands that should 3-bet", "Premium hands make more money as a 3-bet; build the pot when you are ahead."},
      {"loose-call", "Calling without the right price", "Compare your equity with the pot odds before calling. The panel shows the equity you need."},
      {"overfold", "Folding too often vs bets", "If you fold more than the bet size implies, opponents profit by bluffing you. Bots will notice."},
      {"missed-value", "Missing value bets", "When worse hands will call, bet. Checking strong hands lets opponents realise equity for free."},
      {"missed-bluff", "Missing good bluffs", "Weak hands that cannot win at showdown are often the best bluffs when villain folds enough."},
      {"bad-bluff", "Bluffing in bad spots", "Bluff when villain's range is weak or capped and they fold often. Avoid bluffing calling stations."},
      {"passive", "Calling where raising is better", "Strong hands and good semi-bluffs often prefer raising for value and fold equity."},
      {"sizing", "Suboptimal bet sizing", "Size up with strong hands vs sticky players, size down for thin value and cheap bluffs."},
      {"bad-allin", "Poor all-in decisions", "In all-in spots compare equity vs the opponent's range with the price you are getting."},
  };
  return t;
}

const LeakTag* leakTag(const std::string& key) {
  for (auto& t : leakTags())
    if (t.key == key) return &t;
  return nullptr;
}

std::string fmtPct(double x, int digits) {
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(digits);
  o << x * 100 << "%";
  return o.str();
}

std::string fmtChips(double x) {
  long v = std::lround(x);
  return (v >= 0 ? "+" : "\xE2\x88\x92") + std::to_string(std::labs(v));
}

std::string sizeName(double f) {
  static const std::vector<std::pair<double, const char*>> named = {{0.25, "1/4 pot"}, {0.33, "1/3 pot"}, {0.5, "1/2 pot"}, {0.66, "2/3 pot"},
                                                                    {0.75, "3/4 pot"}, {1.0, "pot"},     {1.5, "1.5x pot"}, {2.0, "2x pot"}};
  auto best = named[0];
  for (auto& n : named)
    if (std::fabs(n.first - f) < std::fabs(best.first - f)) best = n;
  if (std::fabs(best.first - f) < 0.06) return best.second;
  return std::to_string(static_cast<int>(std::lround(f * 100))) + "% pot";
}

std::string actionLabel(Act t, int amount, const View& v) {
  switch (t) {
    case Act::Fold: return "Fold";
    case Act::Check: return "Check";
    case Act::Call: return "Call " + std::to_string(amount ? amount : v.legal.toCall);
    case Act::Bet:
    case Act::Raise:
    case Act::AllIn: {
      if (t == Act::AllIn || amount >= v.legal.maxTo) return "All-in";
      if (t == Act::Bet && v.potTotal > 0) return "Bet " + std::to_string(amount) + " (" + sizeName(static_cast<double>(amount - v.me.bet) / v.potTotal) + ")";
      return "Raise to " + std::to_string(amount);
    }
    default: return actName(t);
  }
}

static Act norm(Act a) { return a == Act::Bet ? Act::Raise : a; }

Spot analyzeSpot(const Table& t, int seat, RangeTracker& tracker, Rng& rng, int iterations) {
  Spot s;
  s.view = buildView(t, seat);
  const View& v = s.view;
  s.info = analyzeHand({v.hole[0], v.hole[1]}, v.board);
  s.pot = v.potTotal;
  s.toCall = v.legal.toCall;
  s.needed = s.toCall > 0 ? static_cast<double>(s.toCall) / (s.pot + s.toCall) : 0;
  s.spr = static_cast<double>(effectiveStack(v)) / std::max(1, s.pot);
  s.ip = isInPosition(v);
  s.cardsToCome = v.street == Street::Flop ? 2 : v.street == Street::Turn ? 1 : 0;
  s.isPreflop = v.street == Street::Preflop;
  for (auto* o : liveOpponents(v)) s.oppRanges.push_back({o->seat, o->name, tracker.width(o->seat)});
  const Profile& coach = coachProfile();
  if (s.isPreflop) {
    s.preflop = preflopStrategy(v, &tracker, coach, rng, iterations);
    if (s.preflop.meta.equity >= 0) s.equity = s.preflop.meta.equity;
    else {
      std::vector<Opponent> os;
      for (auto* o : liveOpponents(v)) {
        Opponent x;
        x.range = &tracker.range(o->seat);
        os.push_back(x);
      }
      s.equity = calcEquity(v.hole, {}, os, 1000, rng).equity;
    }
    const Option* best = &s.preflop.options[0];
    for (auto& o : s.preflop.options)
      if (o.w > best->w) best = &o;
    s.recommended = {best->type, best->amount};
    s.recommendedLabel = actionLabel(best->type, best->amount, v);
    for (auto& o : s.preflop.options)
      if (o.w > 0.04) s.mix.push_back({actionLabel(o.type, o.amount, v), o.w});
  } else {
    s.postflop = postflopOptions(v, tracker, coach, rng, iterations);
    s.equity = s.postflop.equity;
    const Option* best = &s.postflop.options[0];
    for (auto& o : s.postflop.options)
      if (o.ev > best->ev) best = &o;
    s.recommended = {best->type, best->amount};
    s.recommendedLabel = actionLabel(best->type, best->amount, v);
    s.bestEV = best->ev;
  }
  return s;
}

Grade gradeDecision(const Spot& spot, Action action) {
  const View& v = spot.view;
  const int bb = v.bb;
  const double P = v.potTotal;
  Grade g;
  g.street = v.street;
  g.equity = spot.equity;
  g.needed = spot.needed;
  g.bestLabel = spot.recommendedLabel;
  Act heroType = action.type;
  int heroAmount = action.amount;
  if (heroType == Act::AllIn) {
    if (v.legal.canBet || v.legal.canRaise) {
      heroType = v.legal.canBet ? Act::Bet : Act::Raise;
      heroAmount = v.legal.maxTo;
    } else
      heroType = v.legal.toCall > 0 ? Act::Call : Act::Check;
  }
  if (heroType == Act::Call && v.legal.toCall == 0) heroType = Act::Check;
  g.heroLabel = actionLabel(heroType, heroType == Act::Call ? v.legal.toCall : heroAmount, v);
  const Act hn = norm(heroType);

  const HandAnalysis& info = spot.info;
  std::string handLine = info.detail.empty() ? info.name : info.name + " - " + info.detail;
  if (!info.draws.description.empty()) handLine += " + " + info.draws.text();
  g.lines.push_back("Your hand: " + handLine + ".");

  if (spot.isPreflop) {
    const PreflopResult& st = spot.preflop;
    g.lines.insert(g.lines.end(), st.notes.begin(), st.notes.end());
    std::map<Act, double> byType;
    for (auto& o : st.options) byType[norm(o.type)] += o.w;
    if (hn == Act::Check && !byType.count(Act::Check)) byType[Act::Check] = byType[Act::Call];
    double wHero = byType.count(hn) ? byType[hn] : 0;
    Act bestType = Act::Fold;
    double bestW = -1;
    for (auto& [t, w] : byType)
      if (w > bestW) {
        bestW = w;
        bestType = t;
      }
    if (st.evBased) {
      std::map<Act, double> evs;
      for (auto& o : st.options) evs[norm(o.type)] = o.ev;
      double bestEV = -1e18;
      for (auto& [t, e] : evs) bestEV = std::max(bestEV, e);
      double heroEV = evs.count(hn) ? evs[hn] : evs[Act::Call];
      g.hasEvLoss = true;
      g.evLoss = std::max(0.0, bestEV - heroEV);
      std::string line = "EV (relative to folding):";
      for (auto& [t, e] : evs) line += std::string(" ") + actName(t) + " " + fmtChips(e) + ";";
      g.lines.push_back(line);
    }
    if (wHero >= 0.45 || wHero >= bestW - 0.08) g.grade = hn == bestType ? GradeLevel::Best : GradeLevel::Good;
    else if (wHero >= 0.18) g.grade = GradeLevel::Good;
    else if (wHero >= 0.05) g.grade = GradeLevel::Inaccuracy;
    else g.grade = GradeLevel::Mistake;
    double hp = st.meta.pct;
    if (hn == Act::Fold && hp <= 0.035 && v.legal.toCall < (v.me.stack + v.me.bet) * 0.5) g.grade = GradeLevel::Blunder;
    if (hn != Act::Fold && hn != Act::Check && hp > 0.8 && bestType == Act::Fold && st.meta.ctx.raises >= 1) g.grade = GradeLevel::Blunder;
    if (g.hasEvLoss && g.grade != GradeLevel::Best) {
      double lossBB = g.evLoss / bb;
      g.grade = lossBB < 0.5 ? GradeLevel::Good : lossBB < 2 ? GradeLevel::Inaccuracy : lossBB < 6 ? GradeLevel::Mistake : GradeLevel::Blunder;
      if (st.evBased) g.tags.push_back("bad-allin");
    }
    if (g.grade != GradeLevel::Best && g.grade != GradeLevel::Good) {
      const PreflopCtx& ctx = st.meta.ctx;
      if (hn == Act::Fold) g.tags.push_back("preflop-too-tight");
      else if (hn == Act::Call && ctx.raises == 0 && v.me.position != "BB" && v.me.position != "SB") g.tags.push_back("open-limp");
      else if (hn == Act::Call && bestType == Act::Raise) g.tags.push_back("passive-preflop");
      else if (bestType == Act::Fold) g.tags.push_back("preflop-too-loose");
    }
    std::string mix = "Coach strategy: ";
    for (size_t i = 0; i < spot.mix.size(); ++i) mix += (i ? " / " : "") + spot.mix[i].first + " " + fmtPct(spot.mix[i].second);
    g.lines.push_back(mix + ".");
  } else {
    const PostflopResult& res = spot.postflop;
    const auto& opts = res.options;
    const Option* best = &opts[0];
    for (auto& o : opts)
      if (o.ev > best->ev) best = &o;
    Option heroOpt;
    bool found = false;
    if (hn == Act::Raise) {
      for (auto& o : opts)
        if (norm(o.type) == Act::Raise && o.amount == std::min(heroAmount, v.legal.maxTo)) {
          heroOpt = o;
          found = true;
        }
      if (!found) {
        Rng rng(static_cast<uint64_t>(heroAmount) * 7919 + v.handId);
        heroOpt = evalAmount(res, heroAmount, rng);
        found = true;
      }
    } else {
      for (auto& o : opts)
        if (norm(o.type) == hn) {
          heroOpt = o;
          found = true;
        }
      if (!found) heroOpt = opts[0];
    }
    g.hasEvLoss = true;
    g.evLoss = std::max(0.0, best->ev - heroOpt.ev);
    double rel = g.evLoss / std::max(P, 4.0 * bb);
    double lossBB = g.evLoss / bb;
    bool sameType = norm(best->type) == hn;
    bool sameSize = !sameType || hn != Act::Raise || std::abs(best->amount - heroAmount) <= std::max<double>(bb, best->amount * 0.15);
    if (sameType && sameSize) g.grade = GradeLevel::Best;
    else if (rel < 0.04 || lossBB < 0.35) g.grade = GradeLevel::Good;
    else if (rel < 0.14) g.grade = GradeLevel::Inaccuracy;
    else if (rel < 0.35) g.grade = GradeLevel::Mistake;
    else g.grade = GradeLevel::Blunder;

    g.lines.push_back(std::string("Equity vs ") + (spot.oppRanges.size() > 1 ? "their estimated ranges: " : "the estimated range: ") + fmtPct(res.equity) +
                      (spot.cardsToCome ? "." : " (river: no more cards)."));
    if (v.legal.toCall > 0) {
      std::string l = "Pot odds: call " + std::to_string(v.legal.toCall) + " to win " + std::to_string(static_cast<int>(P)) + " -> you need " + fmtPct(spot.needed) + " equity";
      if (res.real < 0.95) l += " (out of position you realise only ~" + fmtPct(res.real) + " of it)";
      g.lines.push_back(l + ".");
    }
    if (spot.cardsToCome && info.outs.drawOuts > 0) {
      std::string l = "Draw: " + std::to_string(info.outs.drawOuts) + " outs to a straight/flush = " + fmtPct(outsToEquity(info.outs.drawOuts, 1)) + " next card";
      if (spot.cardsToCome == 2) l += ", " + fmtPct(outsToEquity(info.outs.drawOuts, 2)) + " by the river (rule of 2 and 4)";
      g.lines.push_back(l + ".");
    }
    std::vector<Option> sorted = opts;
    std::sort(sorted.begin(), sorted.end(), [](const Option& a, const Option& b) { return a.ev > b.ev; });
    std::string evl = "EV estimates (chips, vs folding): ";
    for (size_t i = 0; i < sorted.size() && i < 4; ++i) evl += (i ? " / " : "") + actionLabel(sorted[i].type, sorted[i].amount, v) + " " + fmtChips(sorted[i].ev);
    g.lines.push_back(evl + ".");
    if (norm(best->type) == Act::Raise) {
      int add = best->amount - v.me.bet;
      g.lines.push_back("Best: " + actionLabel(best->type, best->amount, v) + " - villain folds ~" + fmtPct(best->foldAll) + "; when called you have ~" + fmtPct(best->eqCalled) +
                        ". A pure bluff needs " + fmtPct(add / (P + add)) + " folds to break even.");
    } else
      g.lines.push_back("Best: " + actionLabel(best->type, best->amount, v) + ".");

    if (g.grade != GradeLevel::Best && g.grade != GradeLevel::Good) {
      Act bt = norm(best->type);
      if (hn == Act::Fold && (bt == Act::Call || bt == Act::Raise)) g.tags.push_back("overfold");
      else if (hn == Act::Call && bt == Act::Fold) g.tags.push_back("loose-call");
      else if (hn == Act::Check && bt == Act::Raise) g.tags.push_back(best->eqCalled >= 0.5 ? "missed-value" : "missed-bluff");
      else if (hn == Act::Raise && bt == Act::Raise) g.tags.push_back("sizing");
      else if (hn == Act::Call && bt == Act::Raise) g.tags.push_back(best->eqCalled >= 0.5 ? "passive" : "missed-bluff");
      else if (hn == Act::Raise) g.tags.push_back("bad-bluff");
    }
    for (auto& t : g.tags) {
      if (t == "overfold") g.lines.push_back("Folding gives up a profitable call: the price is good enough for your equity.");
      if (t == "loose-call") g.lines.push_back("Calling loses money long-term: your equity is below the price you are getting.");
      if (t == "missed-value") g.lines.push_back("You checked a hand that is ahead of villain's calling range: bet for value.");
    }
  }
  g.evLossBB = g.evLoss / bb;
  return g;
}


// ---------- beginner helpers ----------

std::string plainAction(Act t) {
  switch (t) {
    case Act::Fold: return "give up this hand";
    case Act::Check: return "pass for free";
    case Act::Call: return "match the bet";
    case Act::Bet: return "put chips in first";
    case Act::Raise: return "bet more than them";
    case Act::AllIn: return "bet all your chips";
    default: return "";
  }
}

static std::string lower(std::string s) {
  if (!s.empty()) s[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
  return s;
}

PlainAdvice beginnerAdvice(const Spot& spot) {
  PlainAdvice a;
  const View& v = spot.view;
  static const char* words[] = {"Weak", "So-so", "Decent", "Strong", "Very strong"};
  if (spot.isPreflop) {
    double p = spot.preflop.meta.pct;
    a.level = p <= 0.05 ? 4 : p <= 0.15 ? 3 : p <= 0.35 ? 2 : p <= 0.6 ? 1 : 0;
  } else {
    double e = spot.equity;
    a.level = e >= 0.85 ? 4 : e >= 0.65 ? 3 : e >= 0.45 ? 2 : e >= 0.25 ? 1 : 0;
  }
  a.strengthWord = words[a.level];
  const HandAnalysis& info = spot.info;
  a.handLine = "You have " + (spot.isPreflop ? info.name : lower(info.name)) + (info.detail.empty() || spot.isPreflop ? "" : " (" + info.detail + ")") + ".";
  if (info.detail == "pair on board" || info.detail == "playing the board")
    a.handLine += " Careful: that comes from the shared cards, so every player has it too - your own cards add little.";

  Act t = spot.recommended.type;
  bool allIn = (t == Act::Bet || t == Act::Raise) && spot.recommended.amount >= v.legal.maxTo;
  std::string label = actionLabel(t, spot.recommended.amount, v);
  auto paren = label.find(" (");
  if (paren != std::string::npos) label = label.substr(0, paren);
  a.suggestion = label + " - " + plainAction(allIn ? Act::AllIn : t);

  if (spot.isPreflop) {
    a.reasons.push_back(info.name + " is in the top " + fmtPct(spot.preflop.meta.pct) + " of starting hands.");
    int raises = spot.preflop.meta.ctx.raises;
    if (t == Act::Fold) a.reasons.push_back(raises ? "Someone has already raised, so you need a better hand to play." : "Starting with weak cards loses chips over time - waiting for a better hand is fine.");
    else if (t == Act::Raise) a.reasons.push_back(raises ? "Your hand is strong enough to raise again." : "Good hands play best with a raise: it builds the pot and can win it straight away.");
    else if (t == Act::Call) a.reasons.push_back("Your hand is worth seeing the flop for this price.");
    else if (t == Act::Check) a.reasons.push_back("You are in the big blind and nobody raised, so you see the flop for free.");
    if (v.me.position == "BTN" || v.me.position == "CO") a.reasons.push_back("You act late in this hand (good position), so you can play a few more hands.");
    else if (v.me.position == "UTG" || v.me.position == "UTG+1" || v.me.position == "LJ") a.reasons.push_back("You act early, so many players still act after you - play only good hands.");
  } else {
    a.reasons.push_back("You win about " + fmtPct(spot.equity) + " of the time against the hands your opponents are likely to have.");
    if (v.legal.toCall > 0)
      a.reasons.push_back("It costs " + std::to_string(v.legal.toCall) + " to stay in. That is worth it if you win at least " + fmtPct(spot.needed) + " of the time.");
    if (spot.cardsToCome && info.outs.drawOuts > 0)
      a.reasons.push_back(std::to_string(info.outs.drawOuts) + " cards can complete your " + (info.draws.description.empty() ? std::string("draw") : info.draws.description.front()) + " (about " +
                          fmtPct(outsToEquity(info.outs.drawOuts, spot.cardsToCome)) + (spot.cardsToCome == 2 ? " by the river)." : " on the river)."));
    if (t == Act::Fold) a.reasons.push_back("You are probably behind and the price is too high: folding saves chips.");
    else if (t == Act::Call) a.reasons.push_back("Your chance of winning is good enough for the price.");
    else if (t == Act::Check) a.reasons.push_back(a.level >= 3 ? "Checking can trap opponents into betting." : "Your hand is not strong enough to bet, and checking is free.");
    else {
      for (auto& o : spot.postflop.options)
        if (o.type == t && o.amount == spot.recommended.amount) {
          if (o.eqCalled >= 0.5) a.reasons.push_back("Worse hands will pay you off: bet to win more.");
          else a.reasons.push_back("This is a bluff: opponents fold often enough (about " + fmtPct(o.foldAll) + ") to make it profitable.");
        }
    }
  }
  return a;
}

std::string plainSummary(const Grade& g) {
  std::string better = lower(g.bestLabel);
  auto paren = better.find(" (");
  if (paren != std::string::npos) better = better.substr(0, paren);
  std::string why;
  for (auto& t : g.tags) {
    if (t == "overfold") why = " You had a good enough chance to win for the price.";
    else if (t == "loose-call") why = " The price was too high for your chance of winning.";
    else if (t == "missed-value") why = " Your hand was strong - betting makes worse hands pay you.";
    else if (t == "missed-bluff") why = " A bet would have made better hands fold often enough.";
    else if (t == "bad-bluff") why = " Your opponents were unlikely to fold here.";
    else if (t == "preflop-too-loose") why = " This starting hand is too weak to play from here.";
    else if (t == "preflop-too-tight") why = " This starting hand is good enough to play from here.";
    else if (t == "open-limp") why = " When nobody has bet yet, raise or fold instead of just calling.";
    else if (t == "passive-preflop" || t == "passive") why = " A strong hand makes more money by raising.";
    else if (t == "sizing") why = " The bet size made a difference.";
    else if (t == "bad-allin") why = " Your chance of winning did not justify the all-in price.";
  }
  switch (g.grade) {
    case GradeLevel::Best: return "Great move!";
    case GradeLevel::Good: return "Good move.";
    case GradeLevel::Inaccuracy: return "Okay, but " + better + " was slightly better." + why;
    case GradeLevel::Mistake: return "Mistake: " + better + " was better." + why;
    default: return "Big mistake: " + better + " was much better." + why;
  }
}

}  // namespace hl
