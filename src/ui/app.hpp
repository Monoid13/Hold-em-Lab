#pragma once
// Application shell: owns the game state and the screens.

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

class LearnScreen {
 public:
  void draw(App& app, Rectangle area);

 private:
  int section_ = 0;
  Scroll scroll_;
};

class RangesScreen {
 public:
  void draw(App& app, Rectangle area);

 private:
  int pos_ = 5;
  bool rankMode_ = false;
  int selected_ = -1;
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
