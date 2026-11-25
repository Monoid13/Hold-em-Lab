#include "train/drills.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "bots/profiles.hpp"
#include "bots/strategy.hpp"
#include "coach/coach.hpp"
#include "core/equity.hpp"
#include "core/evaluator.hpp"
#include "core/handinfo.hpp"
#include "core/ranges.hpp"

namespace hl {

const DrillInfo& drillInfo(DrillKind k) {
  static const DrillInfo info[] = {
      {"preflop", "Preflop decisions", "6-max, 100bb. Choose fold, call or raise. Answers come from the same charts and range logic the coach uses."},
      {"equity", "Equity estimation", "Estimate how often your hand wins against the opponent's exact hand. Builds the intuition behind every call."},
      {"potodds", "Pot odds & bet math", "Required equity to call, break-even bluff frequency, minimum defence frequency and outs to equity."},
      {"outs", "Counting outs", "Count the cards that complete a straight or flush (or better) for you."},
      {"showdown", "Read the showdown", "Who wins? Spot the best five-card hand fast: kickers, counterfeits and board plays included."},
  };
  return info[static_cast<int>(k)];
}

static std::vector<Card> dealN(Rng& rng) {
  auto d = fullDeck();
  shuffleVec(d, rng);
  return d;
}

View makePreflopView(const std::string& heroPos, const std::string& openerPos, std::array<Card, 2> hole, int stackBB, int bb) {
  const int n = 6;
  auto labels = positionLabels(n);  // index = orderFromButton = seat
  auto seatOf = [&](const std::string& p) { return static_cast<int>(std::find(labels.begin(), labels.end(), p) - labels.begin()); };
  const int actionOrder[] = {3, 4, 5, 0, 1, 2};
  int heroSeat = seatOf(heroPos);
  int openerSeat = openerPos.empty() ? -1 : seatOf(openerPos);
  int stack = stackBB * bb;
  std::map<int, int> bets = {{1, bb / 2}, {2, bb}};
  View v;
  LogEntry sbE;
  sbE.seat = 1; sbE.type = Act::SB; sbE.amount = bb / 2; sbE.to = bb / 2; sbE.blind = true;
  LogEntry bbE;
  bbE.seat = 2; bbE.type = Act::BB; bbE.amount = bb; bbE.to = bb; bbE.blind = true;
  v.log = {sbE, bbE};
  std::set<int> folded;
  int openTo = static_cast<int>(std::lround(bb * (openerPos == "SB" ? 3.0 : 2.5)));
  for (int s : actionOrder) {
    if (s == heroSeat) break;
    LogEntry e;
    e.seat = s;
    if (s == openerSeat) {
      e.type = Act::Raise;
      e.to = openTo;
      e.amount = openTo - bets[s];
      e.toCall = bb - bets[s];
      bets[s] = openTo;
    } else {
      e.type = Act::Fold;
      folded.insert(s);
    }
    v.log.push_back(e);
  }
  int currentBet = openerSeat >= 0 ? openTo : bb;
  int heroBet = bets.count(heroSeat) ? bets[heroSeat] : 0;
  int potTotal = 0;
  for (auto& kv : bets) potTotal += kv.second;
  for (int seat = 0; seat < n; ++seat) {
    int b = bets.count(seat) ? bets[seat] : 0;
    v.players.push_back({seat, "s" + std::to_string(seat), labels[seat], stack - b, b, b, folded.count(seat) > 0, false, labels[seat], seat, stack});
  }
  v.street = Street::Preflop;
  v.potTotal = potTotal;
  v.currentBet = currentBet;
  v.bb = bb;
  v.sb = bb / 2;
  v.numPlayers = n;
  v.raiseCount = openerSeat >= 0 ? 1 : 0;
  v.me = v.players[heroSeat];
  v.hole = hole;
  v.legal.seat = heroSeat;
  v.legal.toCall = currentBet - heroBet;
  v.legal.canCheck = currentBet == heroBet;
  v.legal.canCall = currentBet > heroBet;
  v.legal.canRaise = true;
  v.legal.minTo = currentBet * 2;
  v.legal.maxTo = stack;
  v.legal.currentBet = currentBet;
  v.legal.pot = potTotal;
  v.legal.myBet = heroBet;
  v.legal.stack = stack - heroBet;
  return v;
}

static std::array<Card, 2> holeNear(Rng& rng, double width) {
  for (int t = 0; t < 200; ++t) {
    auto d = dealN(rng);
    std::array<Card, 2> h{std::max(d[0], d[1]), std::min(d[0], d[1])};
    double p = R().percentile[classOf(h[0], h[1])];
    if (rng.next() < 0.3 || (p > width * 0.45 && p < width * 2.2)) return h;
  }
  auto d = dealN(rng);
  return {std::max(d[0], d[1]), std::min(d[0], d[1])};
}

static DrillQuestion genPreflop(Rng& rng) {
  DrillQuestion q;
  q.kind = DrillKind::Preflop;
  bool vsOpen = rng.next() < 0.5;
  if (!vsOpen) {
    static const char* pos[] = {"LJ", "HJ", "CO", "BTN", "SB"};
    q.heroPos = pos[rng.below(5)];
  } else {
    static const char* order[] = {"LJ", "HJ", "CO", "BTN", "SB", "BB"};
    int oi = rng.below(5);
    q.openerPos = order[oi];
    q.heroPos = order[oi + 1 + rng.below(5 - oi)];
  }
  double width = vsOpen ? rfiWidth(q.openerPos) * 0.6 : rfiWidth(q.heroPos);
  auto hole = holeNear(rng, width);
  View v = makePreflopView(q.heroPos, q.openerPos, hole);
  Rng r2(rng.nextU64());
  PreflopResult st = preflopStrategy(v, nullptr, coachProfile(), r2, 400);
  for (auto& o : st.options) {
    int idx = o.type == Act::Fold ? 0 : (o.type == Act::Call || o.type == Act::Check) ? 1 : 2;
    q.weights[idx] += o.w;
  }
  q.hero = {hole[0], hole[1]};
  int cls = classOf(hole[0], hole[1]);
  q.hand = R().classes[cls].name;
  q.pct = R().percentile[cls];
  q.notes = st.notes;
  if (vsOpen) {
    q.choices = {0, 1, 2};
    q.choiceLabels = {"Fold", "Call", "3-Bet"};
    q.prompt = q.openerPos + " opens to 2.5bb. You are in the " + q.heroPos + " with";
  } else {
    q.choices = {0, 2};
    q.prompt = "Folded to you in the " + q.heroPos + ". You hold";
  }
  return q;
}

static DrillQuestion genEquity(Rng& rng) {
  DrillQuestion q;
  q.kind = DrillKind::Equity;
  auto d = dealN(rng);
  static const Street streets[] = {Street::Preflop, Street::Preflop, Street::Flop, Street::Flop, Street::Turn};
  q.street = streets[rng.below(5)];
  int nb = boardCardsAt(q.street);
  q.hero = {d[0], d[1]};
  q.villain = {d[2], d[3]};
  q.board.assign(d.begin() + 4, d.begin() + 4 + nb);
  auto eq = knownEquity({{d[0], d[1]}, {d[2], d[3]}}, q.board, 40000, rng);
  q.answer = eq[0];
  q.prompt = std::string("What is your equity (win% + half of ties) ") + (nb ? std::string("on this ") + streetName(q.street) : "preflop") + "?";
  return q;
}

static DrillQuestion genPotOdds(Rng& rng) {
  DrillQuestion q;
  q.kind = DrillKind::PotOdds;
  static const int pots[] = {20, 30, 40, 50, 60, 80, 100, 120, 150, 200};
  static const double fracs[] = {0.25, 0.33, 0.5, 0.66, 0.75, 1, 1.5};
  int P = pots[rng.below(10)];
  int B = static_cast<int>(std::lround(P * fracs[rng.below(7)]));
  int type = rng.below(4);
  std::ostringstream ex;
  if (type == 0) {
    q.prompt = "The pot is " + std::to_string(P) + ". Villain bets " + std::to_string(B) + ". What equity do you need to call?";
    q.answer = static_cast<double>(B) / (P + 2 * B);
    ex << "You call " << B << " to win " << P + B << " (pot plus bet): " << B << " / (" << P << " + " << B << " + " << B << ") = " << fmtPct(q.answer) << ".";
  } else if (type == 1) {
    q.prompt = "You bluff " + std::to_string(B) + " into a pot of " + std::to_string(P) + ". How often must villain fold for the bluff to break even?";
    q.answer = static_cast<double>(B) / (P + B);
    ex << "Risk " << B << " to win " << P << ": " << B << " / (" << P << " + " << B << ") = " << fmtPct(q.answer) << ".";
  } else if (type == 2) {
    q.prompt = "Villain bets " + std::to_string(B) + " into " + std::to_string(P) + ". What is your minimum defence frequency so their bluffs don't auto-profit?";
    q.answer = static_cast<double>(P) / (P + B);
    ex << "MDF = pot / (pot + bet) = " << P << " / " << P + B << " = " << fmtPct(q.answer) << ".";
  } else {
    static const int outsOpts[] = {4, 6, 8, 9, 12, 15};
    int outs = outsOpts[rng.below(6)];
    bool two = rng.next() < 0.5;
    q.prompt = std::string("On the ") + (two ? "flop (two cards to come, all-in)" : "turn (one card to come)") + " you have " + std::to_string(outs) + " outs. Roughly what is your equity?";
    q.answer = outsToEquity(outs, two ? 2 : 1, two ? 47 : 46);
    ex.setf(std::ios::fixed);
    ex.precision(1);
    ex << "Rule of " << (two ? 4 : 2) << ": " << outs << " x " << (two ? 4 : 2) << " = " << outs * (two ? 4 : 2) << "%. Exact: " << q.answer * 100 << "%.";
  }
  q.explain = ex.str();
  int correct = static_cast<int>(std::lround(q.answer * 100));
  std::vector<int> opts = {correct};
  std::vector<int> deltas = {-20, -12, -8, 8, 12, 20, 25, -25};
  shuffleVec(deltas, rng);
  for (int dl : deltas) {
    if (opts.size() >= 4) break;
    int v = correct + dl;
    if (v > 2 && v < 98 && std::find(opts.begin(), opts.end(), v) == opts.end()) opts.push_back(v);
  }
  shuffleVec(opts, rng);
  q.options = opts;
  q.answerInt = correct;
  return q;
}

static DrillQuestion genOuts(Rng& rng) {
  DrillQuestion q;
  q.kind = DrillKind::Outs;
  for (int t = 0; t < 4000; ++t) {
    auto d = dealN(rng);
    std::vector<Card> hole = {d[0], d[1]};
    std::vector<Card> board(d.begin() + 2, d.begin() + 2 + (rng.next() < 0.6 ? 3 : 4));
    auto o = countOuts(hole, board);
    std::vector<Card> all = hole;
    all.insert(all.end(), board.begin(), board.end());
    if (o.drawOuts >= 3 && categoryOf(evaluate(all)) < STRAIGHT) {
      q.hero = hole;
      q.board = board;
      q.answerInt = o.drawOuts;
      q.outCards = o.cards;
      q.prompt = "How many outs do you have to make a straight or flush (or better)?";
      return q;
    }
  }
  return q;
}

static DrillQuestion genShowdown(Rng& rng) {
  DrillQuestion q;
  q.kind = DrillKind::Showdown;
  auto d = dealN(rng);
  int np = rng.next() < 0.5 ? 2 : 3;
  for (int i = 0; i < np; ++i) q.hands.push_back({d[i * 2], d[i * 2 + 1]});
  q.board.assign(d.begin() + np * 2, d.begin() + np * 2 + 5);
  int best = -1;
  for (auto& h : q.hands) {
    std::vector<Card> all = h;
    all.insert(all.end(), q.board.begin(), q.board.end());
    int s = evaluate(all);
    q.scores.push_back(s);
    q.names.push_back(describeScore(s));
    best = std::max(best, s);
  }
  int winners = 0, idx = -1;
  for (int i = 0; i < np; ++i)
    if (q.scores[i] == best) {
      ++winners;
      idx = i;
    }
  q.answerInt = winners > 1 ? -1 : idx;
  q.prompt = "Who wins at showdown?";
  return q;
}

DrillQuestion generateDrill(DrillKind kind, Rng& rng) {
  switch (kind) {
    case DrillKind::Preflop: return genPreflop(rng);
    case DrillKind::Equity: return genEquity(rng);
    case DrillKind::PotOdds: return genPotOdds(rng);
    case DrillKind::Outs: return genOuts(rng);
    default: return genShowdown(rng);
  }
}

DrillResult gradeDrill(const DrillQuestion& q, int answer) {
  DrillResult r;
  switch (q.kind) {
    case DrillKind::Preflop: {
      int best = 0;
      for (int i = 0; i < 3; ++i)
        if (q.weights[i] > q.weights[best]) best = i;
      double w = q.weights[std::clamp(answer, 0, 2)];
      r.best = best;
      r.correct = w >= 0.3 || answer == best;
      r.score = static_cast<int>(std::lround(std::min(1.0, w / std::max(0.01, q.weights[best])) * 100));
      std::string strength = q.pct <= 0.5 ? "top " + fmtPct(q.pct) : "bottom " + fmtPct(1 - q.pct + 1.0 / 1326);
      r.explanation.push_back(q.hand + " is in the " + strength + " of starting hands.");
      r.explanation.insert(r.explanation.end(), q.notes.begin(), q.notes.end());
      std::string mix = "Strategy:";
      int mixed = 0;
      for (int i = 0; i < 3; ++i)
        if (q.weights[i] > 0.02) {
          mix += " " + q.choiceLabels[i] + " " + fmtPct(q.weights[i]);
          if (q.weights[i] > 0.15) ++mixed;
        }
      r.explanation.push_back(mix + (mixed > 1 ? " (a mixed spot: either is fine)." : "."));
      break;
    }
    case DrillKind::Equity: {
      double err = std::fabs(answer / 100.0 - q.answer) * 100;
      r.score = std::max(0, static_cast<int>(std::lround(100 - err * 4)));
      r.correct = err <= 7;
      std::ostringstream o;
      o.setf(std::ios::fixed);
      o.precision(1);
      o << "Actual equity: " << q.answer * 100 << "% (you said " << answer << "%). Off by " << err << " points.";
      r.explanation.push_back(o.str());
      if (!q.board.empty()) {
        auto a = analyzeHand(q.hero, q.board);
        auto outs = countOuts(q.hero, q.board, q.villain);
        r.explanation.push_back("You have " + a.name + (a.detail.empty() ? "" : " (" + a.detail + ")") + (outs.drawOuts ? " with " + std::to_string(outs.drawOuts) + " draw outs" : "") + ".");
      } else
        r.explanation.push_back("Benchmarks: pair vs two overcards is about 55/45, a dominated hand (AK vs AQ) about 70/30, pair vs smaller pair about 80/20.");
      break;
    }
    case DrillKind::PotOdds:
      r.correct = answer == q.answerInt;
      r.score = r.correct ? 100 : 0;
      r.explanation.push_back(q.explain);
      break;
    case DrillKind::Outs:
      r.correct = answer == q.answerInt;
      r.score = r.correct ? 100 : std::max(0, 100 - std::abs(answer - q.answerInt) * 25);
      r.explanation.push_back(std::to_string(q.answerInt) + " outs. Rule of 2/4: about " + std::to_string(q.answerInt * (q.board.size() == 3 ? 4 : 2)) + "% to hit" +
                              (q.board.size() == 3 ? " by the river (two cards)." : " on the river."));
      break;
    default: {
      r.correct = answer == q.answerInt;
      r.score = r.correct ? 100 : 0;
      int best = *std::max_element(q.scores.begin(), q.scores.end());
      for (size_t i = 0; i < q.hands.size(); ++i)
        r.explanation.push_back("Player " + std::to_string(i + 1) + ": " + q.names[i] + (q.scores[i] == best ? "  <- best" : ""));
    }
  }
  return r;
}

}  // namespace hl
