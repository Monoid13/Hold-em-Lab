#pragma once
// No-limit Texas Hold'em table engine. UI-agnostic and deterministic given an RNG.

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/cards.hpp"

namespace hl {

enum class Act { Fold, Check, Call, Bet, Raise, AllIn, SB, BB, Ante, Uncalled };
enum class Street { Preflop = 0, Flop, Turn, River, Showdown };

const char* streetName(Street s);
const char* actName(Act a);
inline int streetIndex(Street s) { return static_cast<int>(s); }
int boardCardsAt(Street s);

struct SeatPlayer {
  bool occupied = false;
  std::string id, name;
  int stack = 0;
  bool sittingOut = false;
  bool isHero = false;
};

struct HandPlayer {
  bool inHand = false;
  int seat = -1;
  std::string id, name;
  int startStack = 0, stack = 0;
  std::array<Card, 2> hole{};
  int bet = 0, contrib = 0;
  bool folded = false, allIn = false, hasActed = false, raiseLocked = false;
  std::string position;
  int orderFromButton = 0;
  int net = 0;
};

struct LogEntry {
  Street street = Street::Preflop;
  int seat = -1;
  std::string id;
  Act type = Act::Check;
  int amount = 0;  // chips added
  int to = 0;      // total bet this street after the action
  bool blind = false;
  int potBefore = 0, toCall = 0, raiseCount = 0, potAfter = 0;
  bool allIn = false, fullRaise = false;
};

struct Pot {
  int amount = 0;
  std::vector<int> eligible, winners;
};

struct ShowdownHand {
  int seat;
  std::array<Card, 2> hole;
  int score;
  std::string handName;
  std::vector<Card> bestCards;
};

struct Results {
  bool uncontested = false;
  std::vector<ShowdownHand> showdown;
  std::vector<Pot> pots;
  std::map<int, int> winnings;
};

struct Legal {
  int seat = -1, toCall = 0;
  bool canCheck = false, canCall = false, canBet = false, canRaise = false;
  int minTo = 0, maxTo = 0, pot = 0, currentBet = 0, myBet = 0, stack = 0;
};

struct Action {
  Act type = Act::Check;
  int amount = 0;  // raise-to total for bets/raises
};

struct Hand {
  int id = 0;
  int buttonSeat = -1, sbSeat = -1, bbSeat = -1;
  int sb = 1, bb = 2, ante = 0;
  std::vector<Card> deck;
  int deckPos = 0;
  std::vector<Card> board;
  Street street = Street::Preflop;
  std::vector<HandPlayer> players;  // indexed by seat
  std::vector<int> order;           // seats clockwise from the button
  int pot = 0, currentBet = 0, minRaise = 0, actor = -1, lastAggressor = -1, raiseCount = 0;
  std::array<int, 4> streetAggressor{-1, -1, -1, -1};
  std::vector<LogEntry> log;
  bool finished = false;
  Results results;
  int runoutFrom = -1;
  HandPlayer& p(int seat) { return players[seat]; }
  const HandPlayer& p(int seat) const { return players[seat]; }
};

class Table {
 public:
  Table(int numSeats, int sb, int bb, int ante, Rng rng);

  std::vector<SeatPlayer> seats;
  int numSeats;
  int sb, bb, ante;
  int buttonSeat = -1;
  int handNumber = 0;
  bool hasHand = false;
  Hand hand;

  void sit(int seat, const std::string& id, const std::string& name, int stack, bool isHero = false);
  void setBlinds(int sb, int bb, int ante);
  std::vector<int> eligibleSeats() const;
  Hand& startHand();
  std::optional<Legal> legal(int seat) const;
  Legal legal() const { return *legal(hand.actor); }
  LogEntry act(Action a);
  int potTotal() const;
  std::vector<int> liveSeats() const;  // not folded, in hand order
  std::vector<Pot> buildPots() const;
  Rng& rng() { return rng_; }

 private:
  Rng rng_;
  int nextSeat(int from, const std::vector<bool>& ok) const;
  Card draw();
  void post(HandPlayer& hp, int amount, Act kind);
  bool needsAction(int seat) const;
  int nextActor(int from) const;
  bool roundComplete() const;
  void advance(int fromSeat);
  void returnUncalled();
  void collect();
  void endStreet();
  void dealStreet();
  void finishUncontested(int seat);
  void showdown();
  void finish();
};

struct ViewPlayer {
  int seat;
  std::string id, name;
  int stack, bet, contrib;
  bool folded, allIn;
  std::string position;
  int orderFromButton, startStack;
};

/** Sanitized view of the hand for one seat: no other player's hole cards. */
struct View {
  int handId = 0;
  Street street = Street::Preflop;
  std::vector<Card> board;
  int pot = 0, potTotal = 0, currentBet = 0, minRaise = 0, bb = 2, sb = 1, buttonSeat = -1, raiseCount = 0, lastAggressor = -1;
  std::array<int, 4> streetAggressor{-1, -1, -1, -1};
  std::vector<int> order;
  std::vector<LogEntry> log;
  std::vector<ViewPlayer> players;
  ViewPlayer me{};
  std::array<Card, 2> hole{};
  Legal legal;
  int numPlayers = 0;
  const ViewPlayer* player(int seat) const;
};

View buildView(const Table& t, int seat);

}  // namespace hl
