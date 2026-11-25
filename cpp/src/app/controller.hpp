#pragma once
// Game loop: deals hands, asks bots for decisions on human-like timers, waits for the hero,
// runs the coach, records history/stats and handles cash rebuys or tournament blinds.

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/persist.hpp"
#include "bots/bot.hpp"
#include "bots/range_model.hpp"
#include "coach/coach.hpp"
#include "core/table.hpp"

namespace hl {

constexpr const char* HERO_ID = "hero";

struct BotSeat {
  std::string profileKey;
  BotMemory memory;
  std::string name;
  unsigned color = 0x888888;
};

struct ChatBubble {
  std::string text;
  double until = 0;
};

struct GameOver {
  bool active = false, won = false;
  int place = 0, of = 0;
};

struct GameState {
  bool awaitingHero = false;
  std::optional<Spot> spot;
  int thinkingSeat = -1;
  std::map<int, std::string> flashes;
  std::map<int, ChatBubble> chats;
  int revealCount = 5;
  std::map<int, double> allInEquity;
  std::optional<Grade> lastGrade;
  bool handOver = false, heroBusted = false, levelUp = false;
  GameOver gameOver;
  int level = 0, handsThisGame = 0;
  std::vector<std::string> log;
};

class GameController {
 public:
  GameController(Settings& settings, Progress& progress, HistoryStore& history, StatsBook& stats);

  void newGame();
  void update(double nowMs);  // call every frame
  void deal();
  bool heroAct(Action a);
  void rebuyHero();
  void togglePause() { paused_ = !paused_; }
  bool paused() const { return paused_; }
  void saveStats() const;

  Settings& settings;
  Progress& progress;
  HistoryStore& history;
  StatsBook& stats;

  std::unique_ptr<Table> table;
  std::unique_ptr<RangeTracker> tracker;
  std::map<int, BotSeat> bots;
  GameState st;
  int heroSeat = 0;
  int startingStack = 200;
  double now() const { return now_; }

 private:
  struct Event {
    double at;
    uint64_t gen;
    std::function<void()> fn;
  };
  std::vector<Event> events_;
  uint64_t gen_ = 0;
  double now_ = 0;
  bool paused_ = false;
  Rng rng_;
  HandRecord record_;
  const Hand* endingHand_ = nullptr;
  int endingHandId_ = -1;

  void schedule(double delayMs, std::function<void()> fn);
  double speed(double ms) const { return ms * settings.speedFactor(); }
  void applyLevel(int level);
  void next();
  void applyBot(int handId, int seat, const Decision& d);
  void apply(int seat, Action a);
  void handEnd();
  void runoutStep();
  void settle();
  void chat(int seat, const std::string& text);
};

}  // namespace hl
