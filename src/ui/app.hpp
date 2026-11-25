#pragma once
// Application shell: owns the game state and the screens.

#include <chrono>
#include <array>
#include <map>
#include <optional>
#include <string>

#include "app/controller.hpp"
#include "app/persist.hpp"
#include "train/drills.hpp"
#include "ui/gui.hpp"

namespace hl::ui {

class App;

class PlayScreen {
 public:
  void draw(App& app, Rectangle area);
  void handleKeys(App& app);

 private:
  int amount_ = 0;
  std::string amountKey_;
  bool hintShown_ = false;
  std::string hintKey_;
  Scroll sideScroll_;
  Scroll logScroll_;
  size_t lastLogSize_ = 0;
  int animHand_ = -1;
  std::map<std::string, double> cardSeen_;  // card key -> time it started being dealt
  struct Floater { std::string text; double start; };
  std::map<int, std::string> lastFlash_;   // seat -> flash text already seen
  std::map<int, Floater> floaters_;        // seat -> rising action text
  int winAnimHand_ = -1;
  double winAnimStart_ = 0;
  void drawTable(App& app, Rectangle r);
  void drawStatus(App& app, Rectangle r);
  float drawBeginnerPanel(App& app, Rectangle r);
  void drawActionBar(App& app, Rectangle r);
  void drawSidebar(App& app, Rectangle r);
  void perform(App& app, const std::string& act);
  std::string decisionKey(App& app) const;
};

class ReviewScreen {
 public:
  void draw(App& app, Rectangle area);
  void handleKeys(App& app);
  int filter = 0;

 private:
  std::string selected_;
  int step_ = 0;
  Scroll listScroll_, detailScroll_;
};

class StatsScreen {
 public:
  void draw(App& app, Rectangle area);

 private:
  Scroll scroll_;
  int period_ = 0;  // 0 all time, 1 last 100 hands, 2 this session
  long long sessionStartMs_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
};

class DrillsScreen {
 public:
  void draw(App& app, Rectangle area);
  void handleKeys(App& app);
  void open(int kind) {
    active_ = kind;
    next();
  }
  void debugAnswer(App& app) { answer(app, q_ && q_->kind == DrillKind::Equity ? 50 : q_ && q_->kind == DrillKind::Outs ? q_->answerInt : 0); }

 private:
  int active_ = -1;
  std::optional<DrillQuestion> q_;
  std::optional<DrillResult> result_;
  int answered_ = 0;
  int guess_ = 50;
  std::string outsText_;
  Rng rng_ = Rng::fromEntropy();
  Scroll scroll_;
  void next();
  void answer(App& app, int v);
};

float drawRankingsList(Gui& g, float x, float y, float width, float cardW);

struct Block;
class LearnScreen {
 public:
  void draw(App& app, Rectangle area);
  void open(int section) { section_ = section; }

 private:
  int section_ = 0;
  Scroll scroll_, navScroll_;
  float drawBlock(App& app, const Block& b, float x, float y, float W);
};

class RangesScreen {
 public:
  void draw(App& app, Rectangle area);
  void debugSet(int mode, int selected) {
    mode_ = mode;
    selected_ = selected;
  }

 private:
  using Mix = std::array<std::array<double, 3>, 169>;  // raise, call, fold per hand class
  struct HandEq {
    double vsRandom = 0, vsTop10 = 0;
  };
  int mode_ = 0;  // 0 opening, 1 facing a raise, 2 ranking
  int pos_ = 5;
  int heroPos_ = 2, openerPos_ = 1;
  int selected_ = -1;
  std::map<std::string, Mix> mixCache_;
  std::map<int, HandEq> eqCache_;
  const Mix& facingMix(const std::string& hero, const std::string& opener);
  const HandEq& handEquity(int cls);
};

class SettingsScreen {
 public:
  void draw(App& app, Rectangle area);
  void reset() { draftLoaded_ = false; }

 private:
  bool draftLoaded_ = false;
  Settings draft_;
  Scroll scroll_;
  bool confirmWipe_ = false;
};

enum class Tab { Play = 0, Learn, Review, Stats, Drills, Ranges, Settings };
constexpr int TAB_COUNT = 7;

class App {
 public:
  App();
  void frame();
  void startNewGame();

  Gui g;
  Settings settings;
  Progress progress;
  HistoryStore history;
  StatsBook stats;
  GameController ctrl;
  Tab tab = Tab::Play;
  std::string toast;
  bool showRankings = false;
  int welcomePage = 0;
  double toastUntil = 0;
  DrillsScreen& drills() { return drills_; }
  LearnScreen& learn() { return learn_; }
  RangesScreen& ranges() { return ranges_; }
  void showToast(const std::string& t) {
    toast = t;
    toastUntil = GetTime() + 2.5;
  }

 private:
  PlayScreen play_;
  LearnScreen learn_;
  Scroll rankScroll_;
  void drawWelcome();
  void drawRankingsOverlay();
  ReviewScreen review_;
  StatsScreen statsScreen_;
  DrillsScreen drills_;
  RangesScreen ranges_;
  SettingsScreen settingsScreen_;
  void drawTopBar(float h);
};

}  // namespace hl::ui
