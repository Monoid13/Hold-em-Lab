#include <algorithm>

#include "bots/profiles.hpp"
#include "ui/app.hpp"

namespace hl::ui {

namespace {
int indexOf(const std::vector<std::string>& v, const std::string& s) {
  auto it = std::find(v.begin(), v.end(), s);
  return it == v.end() ? 0 : static_cast<int>(it - v.begin());
}
}  // namespace

void SettingsScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  if (!draftLoaded_) {
    draft_ = app.settings;
    draftLoaded_ = true;
  }
  float y0 = scroll_.begin(g, area);
  const float gap = 14;
  float colW = (area.width - 14 - gap * 2) / 3;
  float x1 = area.x, x2 = x1 + colW + gap, x3 = x2 + colW + gap;
  const float pad = 16;
  float fw = colW - pad * 2;

  // --- table settings (applied on new game) ---
  float y = y0 + 44;
  float tableTop = y0;
  std::vector<std::string> lineupKeys, lineupLabels;
  for (auto& l : lineups()) {
    lineupKeys.push_back(l.key);
    lineupLabels.push_back(l.label);
  }
  lineupKeys.push_back("custom");
  lineupLabels.push_back("Custom (choose per seat)");
  int nOpp = draft_.numOpponents;
  int seatRows = (nOpp + 1) / 2;
  float tableH = 44 + 52 + 58 * 2 + 58 + seatRows * 58 + 58 * 4 + 40 + 60;
  g.panel({x1, tableTop, colW, tableH}, "Table", "Applied when you start a new game");
  float x = x1 + pad;
  g.text("Your name", x, y, 14, theme::text2);
  g.textField(1, {x, y + 22, fw, 32}, draft_.heroName, 16);
  y += 64;
  if (g.stepper({x, y, fw, 54}, "Opponents (" + std::to_string(draft_.numOpponents + 1) + "-handed" + (draft_.numOpponents == 1 ? ", heads-up)" : draft_.numOpponents == 5 ? ", 6-max)" : draft_.numOpponents == 8 ? ", full ring)" : ")"),
                draft_.numOpponents, 1, 8, 1)) {
  }
  y += 58;
  int li = indexOf(lineupKeys, draft_.lineup);
  if (g.chooser({x, y, fw, 54}, "Opponent lineup", lineupLabels, li)) draft_.lineup = lineupKeys[li];
  y += 58;
  // seat editors
  const auto& keys = profileKeys();
  std::vector<std::string> labels;
  for (auto& k : keys) labels.push_back(profile(k).label);
  std::vector<std::string> seats = draft_.lineup == "custom" ? draft_.customSeats : lineup(draft_.lineup).seats;
  float sw = (fw - 10) / 2;
  for (int i = 0; i < nOpp; ++i) {
    float sx = x + (i % 2) * (sw + 10), sy = y + (i / 2) * 58;
    int idx = indexOf(keys, seats[i]);
    if (g.chooser({sx, sy, sw, 54}, "Seat " + std::to_string(i + 1), labels, idx)) {
      if (draft_.lineup != "custom") draft_.customSeats = lineup(draft_.lineup).seats;
      draft_.lineup = "custom";
      draft_.customSeats[i] = keys[idx];
    }
  }
  y += seatRows * 58;
  std::vector<std::string> modes = {"cash", "tournament"}, modeLabels = {"Cash game", "Sit & Go tournament (rising blinds)"};
  int mi = indexOf(modes, draft_.mode);
  if (g.chooser({x, y, fw, 54}, "Game", modeLabels, mi)) draft_.mode = modes[mi];
  y += 58;
  g.stepper({x, y, fw, 54}, "Big blind (chips)", draft_.bigBlind, 2, 1000, 2);
  y += 58;
  g.stepper({x, y, fw, 54}, "Starting stack (big blinds)", draft_.startingStackBB, 10, 500, 10);
  y += 58;
  if (draft_.mode == "tournament") g.stepper({x, y, fw, 54}, "Hands per blind level", draft_.levelHands, 3, 50, 1);
  else g.toggle({x, y + 14, fw, 24}, "Auto-rebuy when I bust", draft_.autoRebuy);
  y += 62;
  if (g.button({x, y, fw * 0.66f, 40}, "Start new game with these settings", Btn::Primary, true, 13.5f)) {
    Settings s = draft_;
    // live settings keep their current values
    s.speed = app.settings.speed;
    s.coachMode = app.settings.coachMode;
    s.showHud = app.settings.showHud;
    s.showOppStats = app.settings.showOppStats;
    s.revealProfiles = app.settings.revealProfiles;
    s.showAllCards = app.settings.showAllCards;
    s.fourColor = app.settings.fourColor;
    s.autoDeal = app.settings.autoDeal;
    s.allInEquity = app.settings.allInEquity;
    s.revealInReview = app.settings.revealInReview;
    app.settings = s;
    app.settings.save();
    app.startNewGame();
    app.showToast("New game started");
    draftLoaded_ = false;
  }
  if (g.button({x + fw * 0.66f + 8, y, fw * 0.34f - 8, 40}, "Defaults", Btn::Ghost, true, 13.5f)) draft_ = Settings{};

  // --- training aids (live) ---
  Settings& s = app.settings;
  bool changed = false;
  float ty = y0;
  float th = 44 + 62 + 5 * 52 + 10;
  g.panel({x2, ty, colW, th}, "Training aids", "Applied immediately");
  float yy = ty + 44;
  float xx = x2 + pad;
  std::vector<std::string> coachModes = {"all", "mistakes", "off"}, coachLabels = {"After every decision", "Only when I make a mistake", "Off (still recorded for review)"};
  int ci = indexOf(coachModes, s.coachMode);
  if (g.chooser({xx, yy, fw, 54}, "Coach feedback", coachLabels, ci)) {
    s.coachMode = coachModes[ci];
    changed = true;
  }
  yy += 62;
  changed |= g.toggle({xx, yy, fw, 46}, "Show training panel", s.showHud, "Hand strength, equity vs estimated ranges, pot odds, outs");
  yy += 52;
  changed |= g.toggle({xx, yy, fw, 46}, "Show opponent HUD stats", s.showOppStats, "VPIP / PFR / AF under each seat");
  yy += 52;
  changed |= g.toggle({xx, yy, fw, 46}, "Reveal bot styles", s.revealProfiles, "Show each bot's personality - turn off to practise reading players");
  yy += 52;
  changed |= g.toggle({xx, yy, fw, 46}, "Show equities when all-in", s.allInEquity);
  yy += 52;
  changed |= g.toggle({xx, yy, fw, 46}, "Show all hole cards (practice mode)", s.showAllCards, "See what the bots hold while they play");

  // --- display & pace ---
  float dh = 44 + 62 + 2 * 52 + 10;
  g.panel({x3, ty, colW, dh}, "Display & pace");
  yy = ty + 44;
  xx = x3 + pad;
  std::vector<std::string> speeds = {"instant", "fast", "normal", "realistic"}, speedLabels = {"Instant", "Fast", "Normal", "Realistic (human think times)"};
  int si = indexOf(speeds, s.speed);
  if (g.chooser({xx, yy, fw, 54}, "Bot speed", speedLabels, si)) {
    s.speed = speeds[si];
    changed = true;
  }
  yy += 62;
  bool ad = s.autoDeal;
  if (g.toggle({xx, yy, fw, 46}, "Deal next hand automatically", ad)) {
    s.autoDeal = ad;
    changed = true;
    if (ad && app.ctrl.st.handOver) app.ctrl.deal();
  }
  yy += 52;
  changed |= g.toggle({xx, yy, fw, 46}, "Four-colour deck", s.fourColor, "Clubs green, diamonds blue");

  // --- data ---
  float dy = ty + dh + gap;
  float dataH = 44 + 60 + 50;
  g.panel({x3, dy, colW, dataH}, "Data");
  float ry = dy + 44;
  ry += g.textWrapped("Saved in " + dataDir() + ". Bots remember your tendencies between sessions.", x3 + pad, ry, fw, 12.5f, theme::muted) + 10;
  if (!confirmWipe_) {
    if (g.button({x3 + pad, ry, 200, 36}, "Delete all saved data", Btn::Danger, true, 13.5f)) confirmWipe_ = true;
  } else {
    if (g.button({x3 + pad, ry, 130, 36}, "Yes, delete", Btn::Danger, true, 13.5f)) {
      clearAllData();
      app.progress = Progress{};
      app.history.hands.clear();
      app.stats.resetAll();
      app.settings = Settings{};
      app.settings.save();
      draftLoaded_ = false;
      confirmWipe_ = false;
      app.startNewGame();
      app.showToast("All saved data deleted");
    }
    if (g.button({x3 + pad + 140, ry, 100, 36}, "Cancel", Btn::Ghost, true, 13.5f)) confirmWipe_ = false;
  }
  if (changed) s.save();
  float bottom = std::max({tableTop + tableH, ty + th, dy + dataH});
  scroll_.end(g, bottom - y0 + 20);
}

}  // namespace hl::ui
