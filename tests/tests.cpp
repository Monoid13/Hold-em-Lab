// Dependency-free test runner: `holdem_tests` (or `ctest`).
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "bots/bot.hpp"
#include "bots/profiles.hpp"
#include "bots/range_model.hpp"
#include "bots/stats.hpp"
#include "coach/coach.hpp"
#include "core/equity.hpp"
#include "core/evaluator.hpp"
#include "core/handinfo.hpp"
#include "core/ranges.hpp"
#include "core/table.hpp"
#include "train/drills.hpp"
#include "app/controller.hpp"
#include "app/persist.hpp"
#include <cstdlib>

using namespace hl;

namespace {
int g_failures = 0;
std::vector<std::pair<std::string, std::function<void()>>>& registry() {
  static std::vector<std::pair<std::string, std::function<void()>>> r;
  return r;
}
struct Reg {
  Reg(const char* n, std::function<void()> f) { registry().emplace_back(n, std::move(f)); }
};
#define TEST(name)                       \
  static void name();                    \
  static Reg reg_##name(#name, name);    \
  static void name()
#define CHECK(cond)                                                                   \
  do {                                                                                \
    if (!(cond)) {                                                                    \
      std::printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
      ++g_failures;                                                                   \
    }                                                                                 \
  } while (0)
#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                              \
    auto va = (a);                                                                                  \
    auto vb = (b);                                                                                  \
    if (!(va == vb)) {                                                                              \
      std::printf("    FAILED %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b,       \
                  static_cast<long long>(va), static_cast<long long>(vb));                         \
      ++g_failures;                                                                                 \
    }                                                                                               \
  } while (0)

int ev(const char* s) { return evaluate(parseCards(s)); }
int totalChips(const Table& t) {
  int s = 0;
  for (auto& p : t.seats) s += p.occupied ? p.stack : 0;
  return s;
}
Table makeTable(int n, std::vector<int> stacks, uint64_t seed) {
  Table t(n, 1, 2, 0, Rng(seed));
  for (int i = 0; i < n; ++i) t.sit(i, "p" + std::to_string(i), "P" + std::to_string(i), i < static_cast<int>(stacks.size()) ? stacks[i] : 200);
  return t;
}
void rig(Hand& h, const char* cards) {
  auto cs = parseCards(cards);
  h.deck.insert(h.deck.begin() + h.deckPos, cs.begin(), cs.end());
}
}  // namespace

TEST(evaluator_exhaustive_category_counts) {
  long counts[9] = {};
  Card h[5];
  for (int a = 0; a < 52; ++a)
    for (int b = a + 1; b < 52; ++b)
      for (int c = b + 1; c < 52; ++c)
        for (int d = c + 1; d < 52; ++d)
          for (int e = d + 1; e < 52; ++e) {
            h[0] = a; h[1] = b; h[2] = c; h[3] = d; h[4] = e;
            counts[categoryOf(evaluate(h, 5))]++;
          }
  const long expected[9] = {1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40};
  for (int i = 0; i < 9; ++i) CHECK_EQ(counts[i], expected[i]);
}

TEST(evaluator_comparisons) {
  CHECK(ev("As Ks Qs Js Ts 2c 3d") > ev("9s Ks Qs Js Ts 2c 3d"));
  CHECK(ev("Ah 2d 3c 4s 5h 9c Kd") < ev("2d 3c 4s 5h 6h 9c Kd"));
  CHECK(ev("Ah Ad Kc Ks 2h 2d Qc") > ev("Ah Ad Kc Ks 2h 2d Jc"));
  CHECK(ev("7h 7d 7c 8s 8h 8d 2c") == ev("8s 8h 8d 7h 7d 2c 3c"));
  CHECK(describeScore(ev("Ts Js Qs Ks As")) == "Royal Flush");
  CHECK(describeScore(ev("Ah 2d 3c 4s 5h")) == "Straight, Five high");
}

TEST(ranges_ranking_and_parsing) {
  CHECK_EQ(R().ranking.size(), static_cast<size_t>(169));
  CHECK(std::fabs(R().percentile[R().ranking[168]] - 1.0f) < 1e-5);
  CHECK_EQ(parseRange("22+").count(), static_cast<size_t>(13));
  CHECK_EQ(parseRange("A2s+").count(), static_cast<size_t>(12));
  CHECK_EQ(parseRange("A5s-A2s").count(), static_cast<size_t>(4));
  CHECK_EQ(parseRange("AK").count(), static_cast<size_t>(2));
  CHECK(rfiWidth("UTG") < rfiWidth("HJ") && rfiWidth("HJ") < rfiWidth("CO") && rfiWidth("CO") < rfiWidth("BTN"));
}

TEST(equity_known_matchups) {
  Rng rng(1);
  auto e = knownEquity({{parseCards("As")[0], parseCards("Ks")[0]}, {parseCards("Qh")[0], parseCards("Qd")[0]}}, {}, 40000, rng);
  CHECK(std::fabs(e[0] - 0.46) < 0.02);
  std::array<Card, 2> aa{parseCards("As")[0], parseCards("Ad")[0]};
  auto r = calcEquity(aa, {}, {Opponent{}}, 20000, rng);
  CHECK(std::fabs(r.equity - 0.852) < 0.015);
}

TEST(hand_analysis_draws) {
  auto a = analyzeHand(parseCards("9c 8c"), parseCards("Tc Jh 2c"));
  CHECK(a.draws.flushDraw && a.draws.oesd);
  CHECK_EQ(a.outs.drawOuts, 15);
  CHECK(analyzeHand(parseCards("Ah Kh"), parseCards("Kd 7h 2h")).detail == "top pair, top kicker");
  CHECK(analyzeHand(parseCards("Qs Qd"), parseCards("Jd 7h 2c")).detail == "overpair");
}

TEST(table_blinds_and_positions) {
  auto t = makeTable(6, {}, 1);
  t.buttonSeat = 5;
  Hand& h = t.startHand();
  CHECK_EQ(h.buttonSeat, 0);
  CHECK_EQ(h.p(1).bet, 1);
  CHECK_EQ(h.p(2).bet, 2);
  CHECK_EQ(h.actor, 3);
  CHECK(h.p(3).position == "LJ");
}

TEST(table_heads_up_order) {
  auto t = makeTable(2, {}, 1);
  t.buttonSeat = 1;
  Hand& h = t.startHand();
  CHECK_EQ(h.actor, 0);
  t.act({Act::Call});
  CHECK_EQ(h.actor, 1);
  t.act({Act::Check});
  CHECK(h.street == Street::Flop);
  CHECK_EQ(t.hand.actor, 1);
}

TEST(table_incomplete_raise_does_not_reopen) {
  auto t = makeTable(3, {200, 200, 31}, 1);
  t.buttonSeat = 2;
  t.startHand();
  t.act({Act::Raise, 3});
  CHECK_EQ(t.hand.p(0).bet, 4);
  t.act({Act::Raise, 24});
  CHECK_EQ(t.hand.minRaise, 20);
  t.act({Act::AllIn});
  CHECK_EQ(t.hand.currentBet, 31);
  CHECK(t.legal().canRaise);
  t.act({Act::Call});
  CHECK(!t.legal().canRaise);
  t.act({Act::Call});
  CHECK(t.hand.street == Street::Flop);
}

TEST(table_side_pots) {
  auto t = makeTable(3, {50, 100, 200}, 7);
  t.buttonSeat = 2;
  Hand& h = t.startHand();
  h.p(0).hole = {parseCards("As")[0], parseCards("Ad")[0]};
  h.p(1).hole = {parseCards("Ks")[0], parseCards("Kd")[0]};
  h.p(2).hole = {parseCards("6c")[0], parseCards("2d")[0]};
  rig(h, "3h 9c Th 4s 5c Jd 2h 8s");
  t.act({Act::AllIn});
  t.act({Act::AllIn});
  t.act({Act::Call});
  CHECK(t.hand.finished);
  CHECK_EQ(t.hand.results.pots.size(), static_cast<size_t>(2));
  CHECK_EQ(t.hand.results.pots[0].amount, 150);
  CHECK_EQ(t.seats[0].stack, 150);
  CHECK_EQ(t.seats[1].stack, 100);
  CHECK_EQ(t.seats[2].stack, 100);
}

TEST(table_random_play_conserves_chips) {
  Rng rng(42);
  auto t = makeTable(6, {}, 3);
  for (int hand = 0; hand < 3000; ++hand) {
    for (auto& p : t.seats)
      if (p.stack == 0) p.stack = 200;
    int before = totalChips(t);
    t.startHand();
    int guard = 0;
    while (!t.hand.finished) {
      Legal l = t.legal();
      double r = rng.next();
      Action a;
      if (r < 0.15 && l.toCall > 0) a = {Act::Fold};
      else if (r < 0.65) a = {l.toCall > 0 ? Act::Call : Act::Check};
      else if (r < 0.95 && (l.canBet || l.canRaise)) a = {Act::Raise, l.minTo + rng.below(l.maxTo - l.minTo + 1)};
      else a = {Act::AllIn};
      t.act(a);
      if (++guard > 200) break;
    }
    CHECK(guard <= 200);
    CHECK_EQ(totalChips(t), before);
    if (!t.hand.results.uncontested) CHECK_EQ(t.hand.board.size(), static_cast<size_t>(5));
  }
}

TEST(range_tracker_narrows_raiser) {
  auto t = makeTable(6, {}, 3);
  Hand& h = t.startHand();
  StatsBook stats;
  RangeTracker tr(h, stats);
  int seat = h.actor;
  tr.observe(t.act({Act::Raise, 5}), t.hand);
  double w = tr.width(seat);
  CHECK(w < 0.35 && w > 0.03);
  const Range& r = tr.range(seat);
  CHECK(r[comboIndex(parseCards("As")[0], parseCards("Ah")[0])] > 0.9f);
  CHECK(r[comboIndex(parseCards("7c")[0], parseCards("2d")[0])] < 0.05f);
}

TEST(bots_play_legally_every_profile) {
  Rng rng(11);
  const auto& keys = profileKeys();
  Table t(static_cast<int>(keys.size()), 1, 2, 0, Rng(12));
  for (size_t i = 0; i < keys.size(); ++i) t.sit(static_cast<int>(i), keys[i], keys[i], 200);
  StatsBook stats;
  std::vector<BotMemory> mem(keys.size());
  for (int hand = 0; hand < 200; ++hand) {
    for (auto& p : t.seats)
      if (p.stack < 50) p.stack = 200;
    Hand& h = t.startHand();
    RangeTracker tr(h, stats);
    int guard = 0;
    while (!t.hand.finished) {
      int seat = t.hand.actor;
      Legal l = t.legal();
      Decision d = decideBot(t, seat, profile(keys[seat]), &mem[seat], tr, rng, 0.4);
      if (d.action.type == Act::Check) CHECK_EQ(l.toCall, 0);
      if (d.action.type == Act::Bet || d.action.type == Act::Raise) CHECK(d.action.amount >= l.minTo && d.action.amount <= l.maxTo);
      CHECK(d.thinkMs >= 200 && d.thinkMs <= 9000);
      tr.observe(t.act(d.action), t.hand);
      if (++guard > 300) break;
    }
    stats.recordHand(t.hand);
  }
  CHECK(*stats.raw("station", "vpip").pct > *stats.raw("nit", "vpip").pct);
}

TEST(coach_grades_folding_the_nuts_badly) {
  Table t(2, 1, 2, 0, Rng(5));
  t.sit(0, "hero", "Hero", 200, true);
  t.sit(1, "v", "V", 200);
  t.buttonSeat = 1;
  Hand& h = t.startHand();
  StatsBook stats;
  RangeTracker tr(h, stats);
  h.p(0).hole = {parseCards("Ah")[0], parseCards("Kh")[0]};
  h.p(1).hole = {parseCards("7c")[0], parseCards("2d")[0]};
  rig(h, "3c Qh Jh Th 4d 2s 5c 9s");
  tr.observe(t.act({Act::Call}), t.hand);
  tr.observe(t.act({Act::Check}), t.hand);
  tr.observe(t.act({Act::Bet, 4}), t.hand);
  CHECK_EQ(t.hand.actor, 0);
  Rng rng(2);
  Spot spot = analyzeSpot(t, 0, tr, rng);
  CHECK(spot.equity > 0.97);
  Grade fold = gradeDecision(spot, {Act::Fold});
  CHECK(fold.grade == GradeLevel::Mistake || fold.grade == GradeLevel::Blunder);
  Grade raise = gradeDecision(spot, {Act::Raise, 16});
  CHECK(raise.grade == GradeLevel::Best || raise.grade == GradeLevel::Good);
  // beginner helpers speak plainly
  PlainAdvice adv = beginnerAdvice(spot);
  CHECK(adv.level == 4);
  CHECK(adv.strengthWord == "Very strong");
  CHECK(adv.suggestion.find("Fold") == std::string::npos);
  CHECK(!adv.reasons.empty());
  CHECK(plainSummary(fold).find("istake") != std::string::npos);
  CHECK(plainSummary(raise).find("move") != std::string::npos);
}

TEST(coach_folds_72o_utg) {
  auto t = makeTable(6, {}, 8);
  t.buttonSeat = 2;
  Hand& h = t.startHand();
  CHECK_EQ(h.actor, 0);
  h.p(0).hole = {parseCards("7c")[0], parseCards("2d")[0]};
  StatsBook stats;
  RangeTracker tr(h, stats);
  Rng rng(3);
  Spot spot = analyzeSpot(t, 0, tr, rng);
  CHECK(spot.recommended.type == Act::Fold);
  CHECK(gradeDecision(spot, {Act::Fold}).grade == GradeLevel::Best);
  PlainAdvice adv = beginnerAdvice(spot);
  CHECK(adv.level == 0);
  CHECK(adv.suggestion.rfind("Fold", 0) == 0);
  GradeLevel g = gradeDecision(spot, {Act::Raise, 5}).grade;
  CHECK(g == GradeLevel::Mistake || g == GradeLevel::Blunder || g == GradeLevel::Inaccuracy);
}

TEST(drills_generate_and_grade) {
  Rng rng(4);
  for (int kind = 0; kind < static_cast<int>(DrillKind::Count); ++kind) {
    for (int i = 0; i < 15; ++i) {
      DrillQuestion q = generateDrill(static_cast<DrillKind>(kind), rng);
      DrillResult r = gradeDrill(q, q.kind == DrillKind::Equity ? 50 : 0);
      CHECK(!r.explanation.empty());
    }
  }
}

static void runGame(GameController& c, int maxHands, int heroStyle) {
  double now = 0;
  Rng rng(9);
  int guard = 0;
  while (c.table->handNumber < maxHands && !c.st.gameOver.active && ++guard < 200000) {
    now += 50;
    c.update(now);
    if (c.st.awaitingHero) {
      Legal l = c.table->legal();
      double r = rng.next();
      Action a{l.toCall > 0 ? Act::Call : Act::Check, 0};
      if (heroStyle == 1 && r < 0.3 && (l.canBet || l.canRaise)) a = {Act::Raise, l.minTo};
      if (heroStyle == 1 && r > 0.85 && l.toCall > 0) a = {Act::Fold, 0};
      if (heroStyle == 2 && l.toCall > 0) a = {Act::Fold, 0};  // tight hero: survives while the blinds climb
      CHECK(c.heroAct(a));
    }
  }
  CHECK(guard < 200000);
}

TEST(controller_cash_and_tournament_games) {
  const char* dir = "holdem_test_data";
#ifdef _WIN32
  _putenv_s("HOLDEMLAB_DATA_DIR", dir);
#else
  setenv("HOLDEMLAB_DATA_DIR", dir, 1);
#endif
  clearAllData();
  Settings s;
  s.speed = "instant";
  Progress progress;
  HistoryStore history;
  StatsBook stats;
  GameController c(s, progress, history, stats);
  c.newGame();
  runGame(c, 40, 1);
  CHECK(c.table->handNumber >= 40);
  CHECK(!history.hands.empty());
  CHECK(progress.totals().decisions > 0);
  // history and stats survive a save/load round trip
  HistoryStore reloaded;
  reloaded.load();
  CHECK_EQ(reloaded.hands.size(), history.hands.size());
  CHECK_EQ(reloaded.hands.back().steps.size(), history.hands.back().steps.size());
  StatsBook st2;
  statsFromJson(st2, loadJson("stats"));
  CHECK_EQ(st2.peek(HERO_ID).hands, stats.peek(HERO_ID).hands);

  s.mode = "tournament";
  s.levelHands = 2;
  s.numOpponents = 3;
  s.startingStackBB = 20;
  c.newGame();
  runGame(c, 400, 2);
  CHECK(c.st.gameOver.active);
  CHECK(c.st.level > 0);
  clearAllData();
}

int main() {
  int failedTests = 0;
  for (auto& [name, fn] : registry()) {
    int before = g_failures;
    std::printf("RUN  %s\n", name.c_str());
    fn();
    bool ok = g_failures == before;
    if (!ok) ++failedTests;
    std::printf("%s %s\n", ok ? "  ok" : "  FAIL", name.c_str());
  }
  std::printf("\n%zu tests, %d failed\n", registry().size(), failedTests);
  return failedTests == 0 ? 0 : 1;
}
