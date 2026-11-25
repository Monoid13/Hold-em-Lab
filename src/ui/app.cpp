#include "ui/app.hpp"

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
  static const char* tabs[] = {"Play", "Review", "Stats", "Drills", "Ranges", "Settings"};
  float x = 190;
  for (int i = 0; i < 6; ++i) {
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

  // keyboard shortcuts per tab
  if (g.focusField < 0) {
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
    case Tab::Review: review_.draw(*this, area); break;
    case Tab::Stats: statsScreen_.draw(*this, area); break;
    case Tab::Drills: drills_.draw(*this, area); break;
    case Tab::Ranges: ranges_.draw(*this, area); break;
    case Tab::Settings: settingsScreen_.draw(*this, area); break;
  }
  drawTopBar(barH);
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

}  // namespace hl::ui
