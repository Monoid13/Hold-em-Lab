#include "core/table.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/evaluator.hpp"
#include "core/ranges.hpp"

namespace hl {

const char* streetName(Street s) {
  switch (s) {
    case Street::Preflop: return "preflop";
    case Street::Flop: return "flop";
    case Street::Turn: return "turn";
    case Street::River: return "river";
    default: return "showdown";
  }
}

const char* actName(Act a) {
  switch (a) {
    case Act::Fold: return "fold";
    case Act::Check: return "check";
    case Act::Call: return "call";
    case Act::Bet: return "bet";
    case Act::Raise: return "raise";
    case Act::AllIn: return "allin";
    case Act::SB: return "sb";
    case Act::BB: return "bb";
    case Act::Ante: return "ante";
    default: return "uncalled";
  }
}

int boardCardsAt(Street s) {
  switch (s) {
    case Street::Preflop: return 0;
    case Street::Flop: return 3;
    case Street::Turn: return 4;
    default: return 5;
  }
}

Table::Table(int n, int sb_, int bb_, int ante_, Rng rng) : seats(n), numSeats(n), sb(sb_), bb(bb_), ante(ante_), rng_(rng) {}

void Table::sit(int seat, const std::string& id, const std::string& name, int stack, bool isHero) {
  SeatPlayer& s = seats.at(seat);
  s.occupied = true;
  s.id = id;
  s.name = name;
  s.stack = stack;
  s.isHero = isHero;
  s.sittingOut = false;
}

void Table::setBlinds(int s, int b, int a) {
  sb = s;
  bb = b;
  ante = a;
}

std::vector<int> Table::eligibleSeats() const {
  std::vector<int> out;
  for (int i = 0; i < numSeats; ++i)
    if (seats[i].occupied && seats[i].stack > 0 && !seats[i].sittingOut) out.push_back(i);
  return out;
}

int Table::nextSeat(int from, const std::vector<bool>& ok) const {
  for (int i = 1; i <= numSeats; ++i) {
    int s = ((from + i) % numSeats + numSeats) % numSeats;
    if (ok[s]) return s;
  }
  return -1;
}

Card Table::draw() { return hand.deck[hand.deckPos++]; }

Hand& Table::startHand() {
  if (hasHand && !hand.finished) throw std::logic_error("hand in progress");
  auto elig = eligibleSeats();
  if (elig.size() < 2) throw std::logic_error("need two players with chips");
  std::vector<bool> ok(numSeats, false);
  for (int s : elig) ok[s] = true;
  buttonSeat = buttonSeat < 0 ? elig[rng_.below(static_cast<int>(elig.size()))] : nextSeat(buttonSeat, ok);

  Hand h;
  h.id = ++handNumber;
  h.buttonSeat = buttonSeat;
  h.sb = sb;
  h.bb = bb;
  h.ante = ante;
  h.deck = fullDeck();
  shuffleVec(h.deck, rng_);
  h.order.push_back(buttonSeat);
  for (int s = nextSeat(buttonSeat, ok); s != buttonSeat; s = nextSeat(s, ok)) h.order.push_back(s);
  auto labels = positionLabels(static_cast<int>(h.order.size()));
  h.players.assign(numSeats, HandPlayer{});
  for (size_t i = 0; i < h.order.size(); ++i) {
    int seat = h.order[i];
    HandPlayer& hp = h.players[seat];
    hp.inHand = true;
    hp.seat = seat;
    hp.id = seats[seat].id;
    hp.name = seats[seat].name;
    hp.startStack = hp.stack = seats[seat].stack;
    hp.position = labels[i];
    hp.orderFromButton = static_cast<int>(i);
  }
  h.minRaise = bb;
  hand = std::move(h);
  hasHand = true;

  if (ante > 0)
    for (int seat : hand.order) {
      HandPlayer& hp = hand.p(seat);
      int a = std::min(ante, hp.stack);
      hp.stack -= a;
      hp.contrib += a;
      hand.pot += a;
      if (hp.stack == 0) hp.allIn = true;
      LogEntry e;
      e.seat = seat;
      e.id = hp.id;
      e.type = Act::Ante;
      e.amount = a;
      e.blind = true;
      hand.log.push_back(e);
    }

  bool headsUp = hand.order.size() == 2;
  hand.sbSeat = headsUp ? hand.order[0] : hand.order[1];
  hand.bbSeat = headsUp ? hand.order[1] : hand.order[2];
  post(hand.p(hand.sbSeat), sb, Act::SB);
  post(hand.p(hand.bbSeat), bb, Act::BB);
  hand.currentBet = bb;
  hand.minRaise = bb;

  std::vector<int> dealOrder;
  if (headsUp) dealOrder = {hand.order[0], hand.order[1]};
  else {
    dealOrder.assign(hand.order.begin() + 1, hand.order.end());
    dealOrder.push_back(hand.order[0]);
  }
  for (int r = 0; r < 2; ++r)
    for (int seat : dealOrder) hand.p(seat).hole[r] = draw();

  int firstPre = headsUp ? hand.order[0] : hand.order[3 % hand.order.size()];
  if (needsAction(firstPre) && !roundComplete()) hand.actor = firstPre;
  else hand.actor = nextActor(firstPre);
  if (hand.actor < 0) endStreet();
  return hand;
}

void Table::post(HandPlayer& hp, int amount, Act kind) {
  int a = std::min(amount, hp.stack);
  hp.stack -= a;
  hp.bet += a;
  hp.contrib += a;
  if (hp.stack == 0) hp.allIn = true;
  LogEntry e;
  e.seat = hp.seat;
  e.id = hp.id;
  e.type = kind;
  e.amount = a;
  e.to = hp.bet;
  e.blind = true;
  hand.log.push_back(e);
}

bool Table::needsAction(int seat) const {
  const HandPlayer& p = hand.p(seat);
  return p.inHand && !p.folded && !p.allIn && (!p.hasActed || p.bet < hand.currentBet);
}

int Table::nextActor(int from) const {
  if (roundComplete()) return -1;
  std::vector<bool> ok(numSeats, false);
  for (int s : hand.order) ok[s] = needsAction(s);
  return nextSeat(from, ok);
}

std::vector<int> Table::liveSeats() const {
  std::vector<int> out;
  for (int s : hand.order)
    if (!hand.p(s).folded) out.push_back(s);
  return out;
}

bool Table::roundComplete() const {
  auto live = liveSeats();
  if (live.size() <= 1) return true;
  std::vector<int> canAct;
  for (int s : live)
    if (!hand.p(s).allIn) canAct.push_back(s);
  if (canAct.empty()) return true;
  if (canAct.size() == 1 && hand.p(canAct[0]).bet >= hand.currentBet) return true;
  for (int s : canAct) {
    const HandPlayer& p = hand.p(s);
    if (!p.hasActed || p.bet != hand.currentBet) return false;
  }
  return true;
}

int Table::potTotal() const {
  int s = hand.pot;
  for (int seat : hand.order) s += hand.p(seat).bet;
  return s;
}

std::optional<Legal> Table::legal(int seat) const {
  if (!hasHand || hand.finished || seat != hand.actor || seat < 0) return std::nullopt;
  const HandPlayer& p = hand.p(seat);
  Legal l;
  l.seat = seat;
  l.toCall = std::min(std::max(0, hand.currentBet - p.bet), p.stack);
  bool othersCanAct = false;
  for (int s : liveSeats())
    if (s != seat && !hand.p(s).allIn) othersCanAct = true;
  l.maxTo = p.bet + p.stack;
  bool canAggress = !p.raiseLocked && othersCanAct && p.stack > l.toCall;
  l.minTo = std::min(hand.currentBet == 0 ? hand.bb : hand.currentBet + hand.minRaise, l.maxTo);
  l.canCheck = l.toCall == 0;
  l.canCall = l.toCall > 0;
  l.canBet = canAggress && hand.currentBet == 0;
  l.canRaise = canAggress && hand.currentBet > 0;
  l.pot = potTotal();
  l.currentBet = hand.currentBet;
  l.myBet = p.bet;
  l.stack = p.stack;
  return l;
}

LogEntry Table::act(Action action) {
  if (!hasHand || hand.finished) throw std::logic_error("no hand in progress");
  const int seat = hand.actor;
  HandPlayer& p = hand.p(seat);
  Legal l = *legal(seat);
  Act type = action.type;
  int to = action.amount;
  if (type == Act::AllIn) {
    if (l.canBet || l.canRaise) {
      type = hand.currentBet == 0 ? Act::Bet : Act::Raise;
      to = l.maxTo;
    } else
      type = l.toCall > 0 ? Act::Call : Act::Check;
  }
  if (type == Act::Call && l.toCall == 0) type = Act::Check;
  if (type == Act::Check && l.toCall > 0) throw std::logic_error("cannot check facing a bet");
  if (type == Act::Bet && hand.currentBet > 0) type = Act::Raise;
  if (type == Act::Raise && hand.currentBet == 0) type = Act::Bet;
  if ((type == Act::Bet || type == Act::Raise) && !(l.canBet || l.canRaise)) type = l.toCall > 0 ? Act::Call : Act::Check;

  LogEntry e;
  e.street = hand.street;
  e.seat = seat;
  e.id = p.id;
  e.type = type;
  e.potBefore = potTotal();
  e.toCall = l.toCall;
  e.raiseCount = hand.raiseCount;

  if (type == Act::Fold) {
    p.folded = true;
  } else if (type == Act::Call) {
    int a = l.toCall;
    p.stack -= a;
    p.bet += a;
    p.contrib += a;
    if (p.stack == 0) p.allIn = true;
    e.amount = a;
    e.to = p.bet;
  } else if (type == Act::Bet || type == Act::Raise) {
    to = std::max(l.minTo, std::min(l.maxTo, to));
    int added = to - p.bet;
    int raiseSize = to - hand.currentBet;
    p.stack -= added;
    p.bet = to;
    p.contrib += added;
    if (p.stack == 0) p.allIn = true;
    bool full = raiseSize >= hand.minRaise;
    for (int s : hand.order) {
      HandPlayer& o = hand.p(s);
      if (s == seat || o.folded || o.allIn) continue;
      if (full) {
        o.hasActed = false;
        o.raiseLocked = false;
      } else if (o.hasActed) {
        o.hasActed = false;
        o.raiseLocked = true;
      }
    }
    if (full) hand.minRaise = raiseSize;
    hand.currentBet = std::max(hand.currentBet, to);
    hand.lastAggressor = seat;
    hand.streetAggressor[streetIndex(hand.street)] = seat;
    if (hand.street == Street::Preflop) ++hand.raiseCount;
    e.amount = added;
    e.to = to;
    e.fullRaise = full;
  }
  e.allIn = p.allIn;
  e.potAfter = potTotal();
  p.hasActed = true;
  hand.log.push_back(e);
  advance(seat);
  return e;
}

void Table::advance(int fromSeat) {
  auto live = liveSeats();
  if (live.size() == 1) {
    finishUncontested(live[0]);
    return;
  }
  if (roundComplete()) {
    endStreet();
    return;
  }
  hand.actor = nextActor(fromSeat);
  if (hand.actor < 0) endStreet();
}

void Table::returnUncalled() {
  int top = -1, second = 0;
  for (int s : hand.order) {
    const HandPlayer& p = hand.p(s);
    if (top < 0 || p.bet > hand.p(top).bet) {
      if (top >= 0) second = std::max(second, hand.p(top).bet);
      top = s;
    } else
      second = std::max(second, p.bet);
  }
  if (top >= 0 && hand.p(top).bet > second) {
    HandPlayer& t = hand.p(top);
    int refund = t.bet - second;
    t.bet -= refund;
    t.contrib -= refund;
    t.stack += refund;
    if (t.stack > 0) t.allIn = false;
    LogEntry e;
    e.street = hand.street;
    e.seat = top;
    e.id = t.id;
    e.type = Act::Uncalled;
    e.amount = refund;
    hand.log.push_back(e);
  }
}

void Table::collect() {
  for (int s : hand.order) {
    hand.pot += hand.p(s).bet;
    hand.p(s).bet = 0;
  }
}

void Table::endStreet() {
  returnUncalled();
  collect();
  auto live = liveSeats();
  if (live.size() == 1) {
    finishUncontested(live[0]);
    return;
  }
  if (hand.street == Street::River) {
    showdown();
    return;
  }
  int canAct = 0;
  for (int s : live)
    if (!hand.p(s).allIn) ++canAct;
  if (canAct <= 1) {
    hand.runoutFrom = static_cast<int>(hand.board.size());
    while (hand.board.size() < 5) dealStreet();
    showdown();
    return;
  }
  dealStreet();
  hand.currentBet = 0;
  hand.minRaise = hand.bb;
  for (int s : hand.order) {
    hand.p(s).hasActed = false;
    hand.p(s).raiseLocked = false;
  }
  hand.actor = nextActor(hand.buttonSeat);
  if (hand.actor < 0) endStreet();
}

void Table::dealStreet() {
  hand.street = static_cast<Street>(streetIndex(hand.street) + 1);
  draw();  // burn
  int n = hand.street == Street::Flop ? 3 : 1;
  for (int i = 0; i < n; ++i) hand.board.push_back(draw());
}

void Table::finishUncontested(int seat) {
  returnUncalled();
  collect();
  int amount = hand.pot;
  hand.p(seat).stack += amount;
  hand.results = Results{};
  hand.results.uncontested = true;
  Pot pot;
  pot.amount = amount;
  pot.eligible = {seat};
  pot.winners = {seat};
  hand.results.pots.push_back(pot);
  hand.results.winnings[seat] = amount;
  finish();
}

std::vector<Pot> Table::buildPots() const {
  std::vector<int> levels;
  for (int s : hand.order)
    if (!hand.p(s).folded) levels.push_back(hand.p(s).contrib);
  std::sort(levels.begin(), levels.end());
  levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
  std::vector<Pot> pots;
  int prev = 0, total = 0, distributed = 0;
  for (int s : hand.order) total += hand.p(s).contrib;
  for (int lvl : levels) {
    Pot pot;
    for (int s : hand.order) {
      int c = hand.p(s).contrib;
      pot.amount += std::max(0, std::min(c, lvl) - std::min(c, prev));
    }
    for (int s : hand.order)
      if (!hand.p(s).folded && hand.p(s).contrib >= lvl) pot.eligible.push_back(s);
    if (pot.amount > 0) {
      distributed += pot.amount;
      pots.push_back(pot);
    }
    prev = lvl;
  }
  if (!pots.empty() && total > distributed) pots.back().amount += total - distributed;
  return pots;
}

void Table::showdown() {
  hand.street = Street::Showdown;
  hand.results = Results{};
  std::map<int, int> scores;
  for (int s : liveSeats()) {
    const HandPlayer& p = hand.p(s);
    std::vector<Card> cards = {p.hole[0], p.hole[1]};
    cards.insert(cards.end(), hand.board.begin(), hand.board.end());
    int sc = evaluate(cards);
    scores[s] = sc;
    hand.results.showdown.push_back({s, p.hole, sc, describeScore(sc), bestFive(cards).cards});
  }
  auto pots = buildPots();
  std::vector<int> leftOfButton(hand.order.begin() + 1, hand.order.end());
  leftOfButton.push_back(hand.order[0]);
  for (auto& pot : pots) {
    int best = -1;
    for (int s : pot.eligible) best = std::max(best, scores[s]);
    for (int s : pot.eligible)
      if (scores[s] == best) pot.winners.push_back(s);
    int share = pot.amount / static_cast<int>(pot.winners.size());
    int rem = pot.amount - share * static_cast<int>(pot.winners.size());
    for (int s : leftOfButton) {
      if (std::find(pot.winners.begin(), pot.winners.end(), s) == pot.winners.end()) continue;
      int amt = share + (rem > 0 ? 1 : 0);
      if (rem > 0) --rem;
      hand.results.winnings[s] += amt;
      hand.p(s).stack += amt;
    }
  }
  hand.results.pots = pots;
  finish();
}

void Table::finish() {
  hand.finished = true;
  hand.actor = -1;
  for (int s : hand.order) {
    HandPlayer& hp = hand.p(s);
    seats[s].stack = hp.stack;
    hp.net = hp.stack - hp.startStack;
  }
}

const ViewPlayer* View::player(int seat) const {
  for (auto& p : players)
    if (p.seat == seat) return &p;
  return nullptr;
}

View buildView(const Table& t, int seat) {
  const Hand& h = t.hand;
  View v;
  v.handId = h.id;
  v.street = h.street;
  v.board = h.board;
  v.pot = h.pot;
  v.potTotal = t.potTotal();
  v.currentBet = h.currentBet;
  v.minRaise = h.minRaise;
  v.bb = h.bb;
  v.sb = h.sb;
  v.buttonSeat = h.buttonSeat;
  v.raiseCount = h.raiseCount;
  v.lastAggressor = h.lastAggressor;
  v.streetAggressor = h.streetAggressor;
  v.order = h.order;
  v.log = h.log;
  for (int s : h.order) {
    const HandPlayer& p = h.p(s);
    v.players.push_back({s, p.id, p.name, p.stack, p.bet, p.contrib, p.folded, p.allIn, p.position, p.orderFromButton, p.startStack});
  }
  v.me = *v.player(seat);
  v.hole = h.p(seat).hole;
  auto l = t.legal(seat);
  if (l) v.legal = *l;
  v.numPlayers = static_cast<int>(h.order.size());
  return v;
}

}  // namespace hl
