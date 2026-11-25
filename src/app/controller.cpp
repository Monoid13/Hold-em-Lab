#include "app/controller.hpp"

#include <algorithm>
#include <chrono>

#include "bots/profiles.hpp"
#include "core/equity.hpp"

namespace hl {

GameController::GameController(Settings& s, Progress& p, HistoryStore& h, StatsBook& sb)
    : settings(s), progress(p), history(h), stats(sb), rng_(Rng::fromEntropy()) {}

void GameController::schedule(double delayMs, std::function<void()> fn) { events_.push_back({now_ + std::max(0.0, delayMs), gen_, std::move(fn)}); }

void GameController::update(double nowMs) {
  now_ = nowMs;
  for (auto it = st.chats.begin(); it != st.chats.end();) it = it->second.until < now_ ? st.chats.erase(it) : std::next(it);
  if (paused_) {
    for (auto& e : events_) e.at += 16;  // freeze timers while paused
    return;
  }
  std::vector<Event> due;
  for (auto it = events_.begin(); it != events_.end();) {
    if (it->at <= now_) {
      if (it->gen == gen_) due.push_back(std::move(*it));
      it = events_.erase(it);
    } else
      ++it;
  }
  std::sort(due.begin(), due.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
  for (auto& e : due) e.fn();
}

void GameController::saveStats() const { saveJson("stats", statsToJson(stats)); }

void GameController::newGame() {
  ++gen_;  // drop pending timers from the previous game
  events_.clear();
  int nOpp = std::clamp(settings.numOpponents, 1, 8);
  int bb = settings.bigBlind;
  table = std::make_unique<Table>(nOpp + 1, bb / 2, bb, 0, Rng(rng_.nextU64()));
  tracker.reset();
  startingStack = settings.startingStackBB * bb;
  table->sit(heroSeat, HERO_ID, settings.heroName.empty() ? "You" : settings.heroName, startingStack, true);
  std::vector<std::string> seats = settings.lineup == "custom" ? settings.customSeats : lineup(settings.lineup).seats;
  auto names = botNames();
  shuffleVec(names, rng_);
  bots.clear();
  for (int i = 0; i < nOpp; ++i) {
    std::string key = seats[i % seats.size()];
    const auto& nm = names[i % names.size()];
    int seat = i + 1;
    table->sit(seat, "bot:" + nm.name + ":" + key, nm.name, startingStack);
    BotSeat b;
    b.profileKey = key;
    b.name = nm.name;
    b.color = nm.color;
    bots[seat] = b;
  }
  st = GameState{};
  endingHand_ = nullptr;
  endingHandId_ = -1;
  if (settings.mode == "tournament") applyLevel(0);
  progress.sessions++;
  chat(1 + rng_.below(nOpp), randomChat("greet", rng_));
  schedule(400, [this] { deal(); });
}

void GameController::applyLevel(int level) {
  const auto& L = blindLevels();
  const auto& lv = L[std::min<size_t>(level, L.size() - 1)];
  int unit = settings.bigBlind / 2;
  table->setBlinds(lv[0] * unit, lv[1] * unit, lv[2] * unit);
  st.level = level;
}

void GameController::deal() {
  if (paused_ || !table) return;
  if (table->hasHand && !table->hand.finished) return;
  st.handOver = false;
  st.flashes.clear();
  st.allInEquity.clear();
  st.revealCount = 5;
  SeatPlayer& hero = table->seats[heroSeat];
  if (settings.mode == "cash") {
    for (int s = 0; s < table->numSeats; ++s) {
      SeatPlayer& p = table->seats[s];
      if (!p.occupied || p.isHero) continue;
      if (p.stack < startingStack / 4) {  // bots rebuy / top up like real cash players
        p.stack = startingStack;
        p.sittingOut = false;
      }
    }
    if (hero.stack <= 0) {
      if (settings.autoRebuy) hero.stack = startingStack;
      else {
        st.heroBusted = true;
        return;
      }
    }
  } else {
    int alive = 0, total = 0;
    for (auto& p : table->seats) {
      if (!p.occupied) continue;
      ++total;
      if (p.stack <= 0) p.sittingOut = true;
      else ++alive;
    }
    if (hero.stack <= 0) {
      st.gameOver = {true, false, alive + 1, total};
      return;
    }
    if (alive == 1) {
      st.gameOver = {true, true, 1, total};
      return;
    }
    int level = st.handsThisGame / std::max(1, settings.levelHands);
    st.levelUp = level != st.level;
    if (st.levelUp) applyLevel(level);
  }
  st.heroBusted = false;
  Hand& h = table->startHand();
  st.handsThisGame++;
  tracker = std::make_unique<RangeTracker>(h, stats);

  record_ = HandRecord{};
  record_.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" + std::to_string(h.id);
  record_.handNo = h.id;
  record_.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  record_.sb = h.sb;
  record_.bb = h.bb;
  record_.ante = h.ante;
  record_.buttonSeat = h.buttonSeat;
  record_.heroSeat = heroSeat;
  for (int s : h.order) {
    const HandPlayer& p = h.p(s);
    RecPlayer rp;
    rp.seat = s;
    rp.id = p.id;
    rp.name = p.name;
    rp.position = p.position;
    rp.startStack = p.startStack;
    rp.hole = p.hole;
    if (bots.count(s)) {
      rp.profile = bots[s].profileKey;
      rp.color = bots[s].color;
    }
    record_.players.push_back(rp);
  }
  record_.steps.push_back(snapshot(*table, "Blinds posted"));
  st.log = {"Hand #" + std::to_string(h.id) + " - blinds " + std::to_string(h.sb) + "/" + std::to_string(h.bb) + (h.ante ? " ante " + std::to_string(h.ante) : "")};
  next();
}

void GameController::next() {
  if (table->hand.finished) {
    handEnd();
    return;
  }
  int seat = table->hand.actor;
  if (seat == heroSeat) {
    st.thinkingSeat = -1;
    st.awaitingHero = true;
    st.spot = analyzeSpot(*table, seat, *tracker, rng_);
    return;
  }
  BotSeat& bot = bots[seat];
  st.thinkingSeat = seat;
  st.awaitingHero = false;
  Decision d = decideBot(*table, seat, profile(bot.profileKey), &bot.memory, *tracker, rng_);
  int handId = table->hand.id;
  double delay = gap(d.thinkMs);
  st.thinkFrom = now_;
  st.thinkUntil = now_ + delay;
  schedule(delay, [this, handId, seat, d] { applyBot(handId, seat, d); });
}

void GameController::applyBot(int handId, int seat, const Decision& d) {
  if (!table->hasHand || table->hand.id != handId || table->hand.finished || table->hand.actor != seat) return;
  if (!d.chat.empty()) chat(seat, d.chat);
  apply(seat, d.action);
}

bool GameController::heroAct(Action a) {
  if (!st.awaitingHero || paused_ || !st.spot) return false;
  st.awaitingHero = false;
  Grade g = gradeDecision(*st.spot, a);
  progress.recordDecision(g);
  RecDecision rd;
  rd.step = static_cast<int>(record_.steps.size());
  rd.street = g.street;
  rd.grade = g.grade;
  rd.heroLabel = g.heroLabel;
  rd.bestLabel = g.bestLabel;
  rd.hasEvLoss = g.hasEvLoss;
  rd.evLossBB = g.evLossBB;
  rd.lines = g.lines;
  rd.tags = g.tags;
  record_.decisions.push_back(rd);
  st.lastGrade = g;
  st.spot.reset();
  apply(heroSeat, a);
  return true;
}

void GameController::apply(int seat, Action a) {
  Hand& h = table->hand;
  Street prevStreet = h.street;
  View before = buildView(*table, seat);
  LogEntry e = table->act(a);
  tracker->observe(e, table->hand);
  std::string label;
  if (e.allIn && e.type != Act::Fold && e.type != Act::Check) label = "All-in " + std::to_string(e.to);
  else label = actionLabel(e.type, e.type == Act::Call ? e.amount : e.to, before);
  st.flashes[seat] = label;
  std::string name = table->hand.p(seat).name;
  st.log.push_back(name + ": " + label);
  Snapshot snap = snapshot(*table, name + ": " + label, seat);
  snap.actStreet = streetIndex(prevStreet);
  snap.added = e.amount;
  record_.steps.push_back(snap);
  st.thinkingSeat = -1;
  if (!table->hand.finished && table->hand.street != prevStreet) {
    st.flashes.clear();
    std::string s = streetName(table->hand.street);
    s[0] = static_cast<char>(std::toupper(s[0]));
    st.log.push_back(s + ": " + cardsPretty(table->hand.board));
    schedule(gap(900), [this] { next(); });  // let new cards land before anyone acts
    return;
  }
  if (table->hand.finished) handEnd();
  else next();
}

void GameController::handEnd() {
  Hand& h = table->hand;
  if (endingHandId_ == h.id && endingHand_ == &h) return;
  endingHand_ = &h;
  endingHandId_ = h.id;
  st.awaitingHero = false;
  st.thinkingSeat = -1;
  if (h.runoutFrom >= 0 && h.runoutFrom < 5 && !h.results.uncontested) {
    std::vector<std::array<Card, 2>> hands;
    std::vector<int> seats;
    for (auto& sd : h.results.showdown) {
      hands.push_back(sd.hole);
      seats.push_back(sd.seat);
    }
    std::vector<Card> b(h.board.begin(), h.board.begin() + h.runoutFrom);
    auto eqs = knownEquity(hands, b, 8000, rng_);
    st.allInEquity.clear();
    for (size_t i = 0; i < seats.size(); ++i) st.allInEquity[seats[i]] = eqs[i];
    st.revealCount = h.runoutFrom;
    schedule(gap(1300), [this] { runoutStep(); });
    return;
  }
  settle();
}

void GameController::runoutStep() {
  st.revealCount = st.revealCount == 0 ? 3 : st.revealCount + 1;
  if (st.revealCount >= 5) {
    st.revealCount = 5;
    schedule(gap(800), [this] { settle(); });
  } else
    schedule(gap(1200), [this] { runoutStep(); });
}

void GameController::settle() {
  Hand& h = table->hand;
  st.revealCount = 5;
  stats.recordHand(h);
  saveStats();
  const Results& res = h.results;
  for (auto& [seat, amt] : res.winnings) {
    const HandPlayer& hp = h.p(seat);
    std::string desc;
    if (!res.uncontested)
      for (auto& sd : res.showdown)
        if (sd.seat == seat) desc = " with " + sd.handName;
    st.log.push_back((hp.id == HERO_ID ? std::string("You win ") : hp.name + " wins ") + std::to_string(amt) + desc);
  }
  for (auto& [seat, bot] : bots) {
    const HandPlayer& hp = h.p(seat);
    if (!hp.inHand) continue;
    const Profile& prof = profile(bot.profileKey);
    double netBB = static_cast<double>(hp.net) / h.bb;
    auto it = st.allInEquity.find(seat);
    bool badBeat = it != st.allInEquity.end() && it->second >= 0.65 && hp.net < 0;
    bot.memory.afterHand(netBB, badBeat, prof);
    if (badBeat && rng_.next() < 0.7) chat(seat, randomChat("badBeat", rng_));
    else if (netBB > 40 && rng_.next() < prof.chat * 3) chat(seat, randomChat("winBig", rng_));
    else if (netBB < -40 && rng_.next() < prof.chat * 2) chat(seat, randomChat("loseBig", rng_));
  }
  const HandPlayer& hero = h.p(heroSeat);
  if (hero.inHand) {
    HandSummary hs;
    hs.id = h.id;
    hs.t = record_.time;
    hs.netBB = static_cast<double>(hero.net) / h.bb;
    hs.position = hero.position;
    hs.c1 = hero.hole[0];
    hs.c2 = hero.hole[1];
    for (auto& e : h.log)
      if (e.seat == heroSeat && e.street == Street::Preflop && (e.type == Act::Call || e.type == Act::Raise)) hs.vpip = true;
    if (!res.uncontested)
      for (auto& sd : res.showdown)
        if (sd.seat == heroSeat) hs.showdown = true;
    progress.recordHand(hs);
    progress.save();
  }
  record_.board = h.board;
  record_.uncontested = res.uncontested;
  record_.winnings = std::map<int, int>(res.winnings.begin(), res.winnings.end());
  for (int s : h.order) record_.nets[s] = h.p(s).net;
  for (auto& sd : res.showdown) record_.showdownNames[sd.seat] = sd.handName;
  Snapshot fin = snapshot(*table, "Result");
  fin.result = true;
  record_.steps.push_back(fin);
  history.add(record_);
  st.handOver = true;
  if (settings.autoDeal) schedule(std::max(speed(res.uncontested ? 2000 : 4500), settings.minBotGapMs() * 2) + (settings.speed == "instant" ? 300 : 0), [this] { deal(); });
}

void GameController::chat(int seat, const std::string& text) {
  if (text.empty()) return;
  st.chats[seat] = {text, now_ + 3800};
}

void GameController::rebuyHero() {
  if (table->seats[heroSeat].stack <= 0) table->seats[heroSeat].stack = startingStack;
  st.heroBusted = false;
  deal();
}

}  // namespace hl
