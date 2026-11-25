#include "ui/app.hpp"

#include <algorithm>
#include <cmath>

namespace hl::ui {

static Settings loadSettingsSafe() { return Settings::load(); }

App::App() : settings(loadSettingsSafe()), ctrl(settings, progress, history, stats) {
  progress.load();
  history.load();
  statsFromJson(stats, loadJson("stats"));
}

void App::startNewGame() {
  ctrl.newGame();
  tab = Tab::Play;
}

void App::drawTopBar(float h) {
  int W = GetScreenWidth();
  DrawRectangle(0, 0, W, static_cast<int>(h), theme::surface);
  DrawLine(0, static_cast<int>(h), W, static_cast<int>(h), theme::border);
  g.text("\xE2\x99\xA0", 18, h / 2 - 13, 24, theme::accent, true);
  g.text("Hold'em Lab", 42, h / 2 - 11, 20, theme::text, true);
  static const char* tabs[] = {"Play", "Learn", "Review", "Stats", "Drills", "Ranges", "Settings"};
  float x = 190;
  for (int i = 0; i < TAB_COUNT; ++i) {
    float w = g.measure(tabs[i], 15, true).x + 26;
    Rectangle r{x, h / 2 - 17, w, 34};
    bool on = static_cast<int>(tab) == i;
    bool hv = g.hover(r);
    if (on || hv) DrawRectangleRounded(r, 0.35f, 8, on ? theme::surface3 : theme::surface2);
    g.textCentered(tabs[i], r, 15, on ? theme::text : theme::text2, on);
    if (hv && g.released) {
      tab = static_cast<Tab>(i);
      if (tab == Tab::Settings) settingsScreen_.reset();
    }
    x += w + 4;
  }
  if (ctrl.table && ctrl.table->hasHand) {
    const Hand& hd = ctrl.table->hand;
    std::string meta = "Hand #" + std::to_string(hd.id) + "  \xC2\xB7  Blinds " + std::to_string(hd.sb) + "/" + std::to_string(hd.bb) +
                       (hd.ante ? " ante " + std::to_string(hd.ante) : "") + (settings.mode == "tournament" ? "  \xC2\xB7  Level " + std::to_string(ctrl.st.level + 1) : "");
    if (x + g.measure(meta, 13).x + 30 < W) g.textRight(meta, W - 18.f, h / 2 - 8, 13, theme::muted);
  }
}

void App::frame() {
  g.fourColor = settings.fourColor;
  g.beginFrame();
  ctrl.update(GetTime() * 1000.0);

  const bool welcome = !settings.seenWelcome;
  if (welcome) ctrl.setPaused(true);
  if (!welcome && IsKeyPressed(KEY_SLASH) && g.focusField < 0) showRankings = !showRankings;
  if (showRankings && IsKeyPressed(KEY_ESCAPE)) showRankings = false;
  const bool overlay = welcome || showRankings;
  g.modalActive = overlay;
  if (overlay) {
    float mw = std::min(760.f, GetScreenWidth() - 60.f), mh = std::min(welcome ? (welcomePage == 0 ? 330.f : 540.f) : 750.f, GetScreenHeight() - 60.f);
    g.modal = {(GetScreenWidth() - mw) / 2, (GetScreenHeight() - mh) / 2, mw, mh};
  }

  // keyboard shortcuts per tab
  if (g.focusField < 0 && !overlay) {
    if (tab == Tab::Play) play_.handleKeys(*this);
    else if (tab == Tab::Review) review_.handleKeys(*this);
    else if (tab == Tab::Drills) drills_.handleKeys(*this);
  }

  BeginDrawing();
  ClearBackground(theme::bg);
  const float barH = 56;
  Rectangle area{14, barH + 12, GetScreenWidth() - 28.f, GetScreenHeight() - barH - 24.f};
  switch (tab) {
    case Tab::Play: play_.draw(*this, area); break;
    case Tab::Learn: learn_.draw(*this, area); break;
    case Tab::Review: review_.draw(*this, area); break;
    case Tab::Stats: statsScreen_.draw(*this, area); break;
    case Tab::Drills: drills_.draw(*this, area); break;
    case Tab::Ranges: ranges_.draw(*this, area); break;
    case Tab::Settings: settingsScreen_.draw(*this, area); break;
  }
  drawTopBar(barH);
  if (welcome) drawWelcome();
  else if (showRankings) drawRankingsOverlay();
  if (!toast.empty() && GetTime() < toastUntil) {
    Vector2 m = g.measure(toast, 15);
    Rectangle r{(GetScreenWidth() - m.x) / 2 - 18, GetScreenHeight() - 64.f, m.x + 36, 38};
    DrawRectangleRounded(r, 1, 10, theme::surface3);
    DrawRectangleRoundedLinesEx(r, 1, 10, 1, theme::border);
    g.textCentered(toast, r, 15, theme::text);
  }
  g.endFrame();
  EndDrawing();
}


void App::drawRankingsOverlay() {
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 160});
  Rectangle m = g.modal;
  g.panel(m, "Hand rankings", "Best at the top. Press ? or Esc to close.");
  if (g.button({m.x + m.width - 44, m.y + 8, 32, 28}, "\xE2\x9C\x95", Btn::Ghost, true, 14)) showRankings = false;
  Rectangle body{m.x + 16, m.y + 46, m.width - 22, m.height - 56};
  float y0 = rankScroll_.begin(g, body);
  float h = drawRankingsList(g, body.x, y0, body.width - 14, 40);
  rankScroll_.end(g, h);
  if (g.released && !CheckCollisionPointRec(g.mouse, m)) showRankings = false;
}

void App::drawWelcome() {
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 170});
  Rectangle m = g.modal;
  g.panel(m);
  float x = m.x + 32, y = m.y + 28, w = m.width - 64;
  auto finish = [this] {
    settings.seenWelcome = true;
    settings.save();
    ctrl.setPaused(false);
  };
  auto para = [&](const std::string& t, float size = 15.f, Color c = theme::text2) { y += g.textWrapped(t, x, y, w, size, c) + 12; };
  auto heading = [&](const std::string& t) {
    g.text(t, x, y, 24, theme::text, true);
    y += 42;
  };
  if (welcomePage == 0) {
    g.text("\xE2\x99\xA0", x, y - 4, 40, theme::accent, true);
    g.text("Welcome to Hold'em Lab", x + 40, y, 26, theme::text, true);
    y += 56;
    para("Learn Texas Hold'em poker by playing against computer opponents. A built-in coach explains what is going on and tells you how good each of your decisions was.", 16);
    para("Have you played poker before?", 16, theme::text);
    y += 8;
    float bw = (w - 16) / 2;
    if (g.button({x, y, bw, 86}, "", Btn::Primary)) {
      settings.beginnerMode = true;
      settings.showSuggestion = true;
      welcomePage = 1;
    }
    g.textCentered("I'm new to poker", {x, y + 14, bw, 26}, 18, theme::accentInk, true);
    g.textCentered("Turn on beginner help and show me around", {x, y + 46, bw, 22}, 13, theme::accentInk);
    if (g.button({x + bw + 16, y, bw, 86}, "")) {
      settings.beginnerMode = false;
      settings.showOppStats = true;
      settings.lineup = "mixed";
      finish();
      startNewGame();
    }
    g.textCentered("I know how to play", {x + bw + 16, y + 14, bw, 26}, 18, theme::text, true);
    g.textCentered("Tougher opponents, advanced stats", {x + bw + 16, y + 46, bw, 22}, 13, theme::text2);
    y += 106;
    para("You can change this any time in Settings.", 13, theme::muted);
    return;
  }
  if (welcomePage == 1) {
    heading("How a hand works");
    para("1.  You get 2 private cards. Up to 5 shared cards are dealt face up in the middle.");
    para("2.  Your hand is the best 5 cards you can make from your 2 cards plus the shared cards.");
    para("3.  There are four betting rounds: pre-flop (no shared cards yet), the flop (3 cards), the turn (4th card) and the river (5th card).");
    para("4.  Win the chips in the middle (the pot) by having the best hand at the end - or by betting so that everyone else folds.");
    float cw = 46;
    auto ex = parseCards("Ah Kd 7c 2s Ks Qh 9d");
    g.text("You", x, y, 13, theme::muted);
    g.text("Shared cards", x + 120, y, 13, theme::muted);
    y += 20;
    for (int i = 0; i < 2; ++i) g.card(ex[i], {x + i * (cw + 4), y, cw, cw * 1.4f});
    for (int i = 2; i < 7; ++i) g.card(ex[i], {x + 120 + (i - 2) * (cw + 4), y, cw, cw * 1.4f});
    g.text("= Pair of Kings, ace kicker", x + 120 + 5 * (cw + 4) + 12, y + cw * 0.5f, 15, theme::accent, true);
    y += cw * 1.4f + 16;
  } else if (welcomePage == 2) {
    heading("Your buttons");
    const char* rows[][2] = {{"Fold", "Give up this hand. You only lose what you already put in."},
                             {"Check", "Stay in without betting. Only possible when nobody has bet."},
                             {"Call", "Put in the same amount as the current bet to stay in."},
                             {"Bet / Raise", "Put in chips (or more chips than the last bet). Others must match it or fold."},
                             {"All-in", "Bet every chip you have."}};
    for (auto& r : rows) {
      g.text(r[0], x, y, 16, theme::accent, true);
      y += g.textWrapped(r[1], x + 130, y, w - 130, 15, theme::text2) + 14;
    }
    para("Keyboard: F fold, C check/call, R bet/raise, H hint, Space next hand.", 13.5f, theme::muted);
  } else {
    heading("Help is always on");
    para("\xE2\x80\xA2  The panel on the right says what you have, how strong it is, and suggests a move - the suggested button is marked with a gold star.");
    para("\xE2\x80\xA2  After each decision the coach tells you whether it was a good move, and why.");
    para("\xE2\x80\xA2  The opponents take their time, and the game always waits for you. Change their speed in Settings.");
    para("\xE2\x80\xA2  Forgot which hand beats which? Click Hand rankings or press ?. The Learn tab explains everything step by step.");
  }
  float by = m.y + m.height - 64;
  if (g.button({x, by, 110, 40}, "Back", Btn::Ghost, true, 14)) --welcomePage;
  if (welcomePage < 3) {
    if (g.button({x + w - 150, by, 150, 40}, "Next", Btn::Primary, true, 15)) ++welcomePage;
    if (g.button({x + w - 250, by, 90, 40}, "Skip", Btn::Ghost, true, 14)) finish();
  } else if (g.button({x + w - 170, by, 170, 40}, "Let's play!", Btn::Primary, true, 15)) {
    finish();
    tab = Tab::Play;
  }
  for (int i = 1; i <= 3; ++i) DrawCircleV({m.x + m.width / 2 - 24 + (i - 1) * 24.f, by + 20}, 5, i == welcomePage ? theme::accent : theme::surface3);
}

}  // namespace hl::ui
