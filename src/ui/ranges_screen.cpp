#include <algorithm>
#include <cmath>

#include "core/ranges.hpp"
#include "ui/app.hpp"

namespace hl::ui {

void RangesScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  const auto& D = R();
  const auto& positions = rfiPositions();
  float infoW = 330;
  float x = area.x, y = area.y;
  if (g.button({x, y, 150, 30}, "Opening ranges", !rankMode_ ? Btn::ChipOn : Btn::Chip, true, 13)) rankMode_ = false;
  if (g.button({x + 158, y, 140, 30}, "Hand ranking", rankMode_ ? Btn::ChipOn : Btn::Chip, true, 13)) rankMode_ = true;
  y += 42;
  const std::string& pos = positions[pos_];
  if (!rankMode_) {
    float px = x;
    for (size_t i = 0; i < positions.size(); ++i) {
      std::string lab = positions[i] == "HU" ? "Heads-up BTN" : positions[i];
      float w = g.measure(lab, 12.5f, true).x + 22;
      if (g.button({px, y, w, 28}, lab, static_cast<int>(i) == pos_ ? Btn::ChipOn : Btn::Chip, true, 12.5f)) pos_ = static_cast<int>(i);
      px += w + 6;
    }
    y += 40;
    g.text("Raise-first-in from " + (pos == "HU" ? std::string("the heads-up button") : pos) + " at 100bb: " + pct(rfiWidth(pos), 1) + " of hands. Highlighted hands are opened; everything else folds.", x, y, 14,
           theme::text2);
  } else
    g.text("Darker = stronger. The ranking blends raw equity with playability; bots and the coach build ranges from it.", x, y, 14, theme::text2);
  y += 30;
  float gridW = std::min(area.width - infoW - 24, area.height - (y - area.y) - 30);
  float cell = gridW / 13;
  const ClassSet* set = rfiSet(pos);
  static const Color seq[] = {{16, 66, 129, 255}, {24, 79, 149, 255}, {28, 92, 171, 255}, {37, 106, 191, 255}, {42, 120, 214, 255}, {61, 79, 99, 255}, {42, 48, 56, 255}};
  for (auto& c : D.classes) {
    Rectangle r{x + c.col * cell, y + c.row * cell, cell - 2, cell - 2};
    Color fill = theme::surface2, ink = theme::muted;
    if (!rankMode_) {
      if (set && set->test(c.index)) {
        fill = theme::series;
        ink = WHITE;
      }
    } else {
      int step = std::min(6, static_cast<int>(D.percentile[c.index] * 7));
      fill = seq[step];
      ink = step <= 4 ? WHITE : Color{200, 208, 216, 255};
    }
    bool hv = g.hover(r);
    DrawRectangleRounded(r, 0.18f, 4, hv ? Color{static_cast<unsigned char>(std::min(255, fill.r + 20)), static_cast<unsigned char>(std::min(255, fill.g + 20)), static_cast<unsigned char>(std::min(255, fill.b + 20)), 255} : fill);
    if (selected_ == c.index) DrawRectangleRoundedLinesEx({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, 0.18f, 4, 2, theme::accent);
    g.textCentered(c.name, r, std::clamp(cell * 0.28f, 8.f, 14.f), ink, true);
    if (hv && g.released) selected_ = c.index;
  }
  g.text("Upper-right triangle: suited \xC2\xB7 diagonal: pairs \xC2\xB7 lower-left: offsuit.", x, y + 13 * cell + 8, 12.5f, theme::muted);

  Rectangle ip{area.x + area.width - infoW, area.y, infoW, area.height};
  g.panel(ip);
  float iy = ip.y + 16;
  if (selected_ >= 0) {
    const HandClass& c = D.classes[selected_];
    g.text(c.name, ip.x + 16, iy, 24, theme::text, true);
    iy += 38;
    iy += g.textWrapped("Rank #" + std::to_string(D.rankPos[selected_] + 1) + " of 169 \xC2\xB7 top " + pct(D.percentile[selected_]) + " of hands \xC2\xB7 " + std::to_string(c.combos) + " combos.", ip.x + 16, iy,
                        infoW - 32, 14, theme::text2) +
          8;
    std::string opens;
    for (auto& p : positions)
      if (rfiSet(p)->test(selected_)) opens += (opens.empty() ? "" : ", ") + (p == "HU" ? std::string("HU button") : p);
    iy += g.textWrapped("Open-raise from: " + (opens.empty() ? std::string("never (fold when first in)") : opens), ip.x + 16, iy, infoW - 32, 14, theme::text) + 16;
  } else {
    g.text("Click a hand for details.", ip.x + 16, iy, 14, theme::muted);
    iy += 36;
  }
  g.text("How to use this", ip.x + 16, iy, 16, theme::text, true);
  iy += 28;
  const char* tips[] = {"Open tighter in early position: more players are left to act behind you.", "The button opens the widest - you are guaranteed position after the flop.",
                        "Facing a raise, continue with a range narrower than the opener's, wider from the big blind (you already have 1bb in).", "Drill these in Drills > Preflop decisions."};
  for (const char* t : tips) {
    g.text("\xE2\x80\xA2", ip.x + 16, iy, 13.5f, theme::muted);
    iy += g.textWrapped(t, ip.x + 30, iy, infoW - 46, 13.5f, theme::text2) + 8;
  }
}

}  // namespace hl::ui
