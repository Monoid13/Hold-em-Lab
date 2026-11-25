// Ranges: opening charts by position, responses to a raise, and the overall hand ranking.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bots/profiles.hpp"
#include "bots/strategy.hpp"
#include "core/equity.hpp"
#include "core/ranges.hpp"
#include "train/drills.hpp"
#include "ui/app.hpp"

namespace hl::ui {

namespace {
const char* HERO_POS[] = {"HJ", "CO", "BTN", "SB", "BB"};
const char* OPEN_POS[] = {"LJ", "HJ", "CO", "BTN", "SB"};
const Color COL_RAISE{230, 180, 80, 255};   // gold
const Color COL_CALL{57, 135, 229, 255};    // blue
const Color COL_FOLD{42, 48, 56, 255};      // dark grey

// A representative concrete combo for a hand class.
std::array<Card, 2> comboFor(const HandClass& c) {
  if (c.type == ClassType::Pair) return {makeCard(c.hi, 3), makeCard(c.hi, 2)};
  if (c.type == ClassType::Suited) return {makeCard(c.hi, 3), makeCard(c.lo, 3)};
  return {makeCard(c.hi, 3), makeCard(c.lo, 2)};
}

int posIndex(const char* const* list, int n, const std::string& p) {
  for (int i = 0; i < n; ++i)
    if (p == list[i]) return i;
  return -1;
}
}  // namespace

const RangesScreen::Mix& RangesScreen::facingMix(const std::string& hero, const std::string& opener) {
  std::string key = hero + "|" + opener;
  auto it = mixCache_.find(key);
  if (it != mixCache_.end()) return it->second;
  Mix mix{};
  Rng rng(42);
  for (auto& c : R().classes) {
    View v = makePreflopView(hero, opener, comboFor(c));
    PreflopResult st = preflopStrategy(v, nullptr, coachProfile(), rng, 200);
    std::array<double, 3> w{};  // raise, call, fold
    for (auto& o : st.options) {
      int k = o.type == Act::Raise ? 0 : (o.type == Act::Call || o.type == Act::Check) ? 1 : 2;
      w[k] += o.w;
    }
    mix[c.index] = w;
  }
  return mixCache_[key] = mix;
}

const RangesScreen::HandEq& RangesScreen::handEquity(int cls) {
  auto it = eqCache_.find(cls);
  if (it != eqCache_.end()) return it->second;
  HandEq e{};
  Rng rng(7 + cls);
  auto hole = comboFor(R().classes[cls]);
  e.vsRandom = calcEquity(hole, {}, {Opponent{}}, 20000, rng).equity;
  Range top10{};
  for (int k = 0; k < NUM_COMBOS; ++k) top10[k] = static_cast<float>(softIn(R().percentile[R().comboClass[k]], 0.10));
  Opponent o;
  o.range = &top10;
  e.vsTop10 = calcEquity(hole, {}, {o}, 20000, rng).equity;
  return eqCache_[cls] = e;
}

void RangesScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  const auto& D = R();
  const auto& positions = rfiPositions();
  float infoW = std::min(360.f, area.width * 0.3f);
  float x = area.x, y = area.y;
  static const char* modes[] = {"Opening ranges", "Facing a raise", "Hand ranking"};
  for (int i = 0; i < 3; ++i) {
    float w = g.measure(modes[i], 13, true).x + 26;
    if (g.button({x, y, w, 30}, modes[i], mode_ == i ? Btn::ChipOn : Btn::Chip, true, 13)) mode_ = i;
    x += w + 8;
  }
  x = area.x;
  y += 42;
  const std::string& pos = positions[pos_];
  const Mix* mix = nullptr;
  std::string legendText;
  if (mode_ == 0) {
    float px = x;
    for (size_t i = 0; i < positions.size(); ++i) {
      std::string lab = positions[i] == "HU" ? "Heads-up BTN" : positions[i];
      float w = g.measure(lab, 12.5f, true).x + 22;
      if (g.button({px, y, w, 28}, lab, static_cast<int>(i) == pos_ ? Btn::ChipOn : Btn::Chip, true, 12.5f)) pos_ = static_cast<int>(i);
      px += w + 6;
    }
    y += 38;
    const ClassSet* set = rfiSet(pos);
    int combos = 0;
    for (auto& c : D.classes)
      if (set->test(c.index)) combos += c.combos;
    legendText = "Raise first in from " + (pos == "HU" ? std::string("the heads-up button") : pos) + " at 100bb: " + std::to_string(combos) + " of 1,326 combos (" + pct(rfiWidth(pos), 1) +
                 "). Gold hands are opened with a raise; everything else folds.";
  } else if (mode_ == 1) {
    // hero seat and opener seat (opener must act before the hero)
    g.text("You are in", x, y + 6, 13, theme::text2);
    float px = x + 80;
    for (int i = 0; i < 5; ++i) {
      float w = g.measure(HERO_POS[i], 12.5f, true).x + 22;
      if (g.button({px, y, w, 28}, HERO_POS[i], heroPos_ == i ? Btn::ChipOn : Btn::Chip, true, 12.5f)) heroPos_ = i;
      px += w + 6;
    }
    // valid openers: those before the hero in pre-flop order LJ HJ CO BTN SB
    int heroOrder = heroPos_;  // HJ=0 -> openers {LJ}; CO=1 -> {LJ,HJ}; ...
    openerPos_ = std::min(openerPos_, heroOrder);
    px += 24;
    g.text("raiser in", px, y + 6, 13, theme::text2);
    px += 70;
    for (int i = 0; i <= heroOrder; ++i) {
      float w = g.measure(OPEN_POS[i], 12.5f, true).x + 22;
      if (g.button({px, y, w, 28}, OPEN_POS[i], openerPos_ == i ? Btn::ChipOn : Btn::Chip, true, 12.5f)) openerPos_ = i;
      px += w + 6;
    }
    y += 38;
    mix = &facingMix(HERO_POS[heroPos_], OPEN_POS[openerPos_]);
    double tot[3] = {};
    for (auto& c : D.classes)
      for (int k = 0; k < 3; ++k) tot[k] += (*mix)[c.index][k] * c.combos;
    legendText = std::string(OPEN_POS[openerPos_]) + " opens to 2.5bb and you are in the " + HERO_POS[heroPos_] + " (100bb, 6-max): 3-bet " + pct(tot[0] / NUM_COMBOS, 1) + ", call " +
                 pct(tot[1] / NUM_COMBOS, 1) + ", fold " + pct(tot[2] / NUM_COMBOS, 1) + " of hands. Split cells are mixed strategies.";
  } else
    legendText = "Darker = stronger. The ranking blends raw equity with playability (suitedness, connectedness); bots and the coach build ranges from it.";
  y += g.textWrapped(legendText, x, y, area.width - infoW - 40, 14, theme::text2) + 10;
  // colour legend
  if (mode_ != 2) {
    struct Lg {
      Color c;
      const char* t;
    } lg[] = {{COL_RAISE, mode_ == 0 ? "Raise" : "3-bet"}, {COL_CALL, "Call"}, {COL_FOLD, "Fold"}};
    float lx = x;
    for (int i = 0; i < 3; ++i) {
      if (mode_ == 0 && i == 1) continue;
      DrawRectangleRounded({lx, y + 3, 14, 14}, 0.3f, 4, lg[i].c);
      g.text(lg[i].t, lx + 20, y + 1, 13, theme::text2);
      lx += g.measure(lg[i].t, 13).x + 44;
    }
    y += 26;
  }
  float gridW = std::min(area.width - infoW - 24, area.height - (y - area.y) - 30);
  float cell = gridW / 13;
  const ClassSet* set = rfiSet(pos);
  static const Color seq[] = {{16, 66, 129, 255}, {24, 79, 149, 255}, {28, 92, 171, 255}, {37, 106, 191, 255}, {42, 120, 214, 255}, {61, 79, 99, 255}, {42, 48, 56, 255}};
  for (auto& c : D.classes) {
    Rectangle r{x + c.col * cell, y + c.row * cell, cell - 2, cell - 2};
    Color ink = WHITE;
    bool hv = g.hover(r);
    if (mode_ == 0) {
      bool in = set && set->test(c.index);
      DrawRectangleRounded(r, 0.18f, 4, in ? COL_RAISE : COL_FOLD);
      ink = in ? theme::accentInk : theme::muted;
    } else if (mode_ == 1) {
      // horizontal split proportional to raise / call / fold
      const auto& w = (*mix)[c.index];
      float sx = r.x;
      Color cols[3] = {COL_RAISE, COL_CALL, COL_FOLD};
      DrawRectangleRounded(r, 0.18f, 4, COL_FOLD);
      for (int k = 0; k < 2; ++k) {
        float sw = static_cast<float>(r.width * w[k]);
        if (sw >= 1) DrawRectangleRec({sx, r.y, sw, r.height}, cols[k]);
        sx += sw;
      }
      int dom = static_cast<int>(std::max_element(w.begin(), w.end()) - w.begin());
      ink = dom == 0 ? theme::accentInk : dom == 1 ? WHITE : theme::muted;
      if (hv) g.setTooltip(c.name + ":  3-bet " + pct(w[0]) + "  \xC2\xB7  call " + pct(w[1]) + "  \xC2\xB7  fold " + pct(w[2]));
    } else {
      int step = std::min(6, static_cast<int>(D.percentile[c.index] * 7));
      DrawRectangleRounded(r, 0.18f, 4, seq[step]);
      ink = step <= 4 ? WHITE : Color{200, 208, 216, 255};
    }
    if (hv) DrawRectangleRoundedLinesEx(r, 0.18f, 4, 1.5f, Color{255, 255, 255, 120});
    if (selected_ == c.index) DrawRectangleRoundedLinesEx({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, 0.18f, 4, 2.5f, theme::text);
    g.textCentered(c.name, r, std::clamp(cell * 0.28f, 8.f, 14.f), ink, true);
    if (hv && g.released) selected_ = c.index;
  }
  g.text("Upper-right triangle: suited \xC2\xB7 diagonal: pairs \xC2\xB7 lower-left: offsuit. Click a hand for details.", x, y + 13 * cell + 8, 12.5f, theme::muted);

  // ---- info panel ----
  Rectangle ip{area.x + area.width - infoW, area.y, infoW, area.height};
  g.panel(ip);
  float iy = ip.y + 16, ix = ip.x + 16, iw = infoW - 32;
  if (selected_ >= 0) {
    const HandClass& c = D.classes[selected_];
    auto hole = comboFor(c);
    g.card(hole[0], {ix, iy, 44, 62});
    g.card(hole[1], {ix + 48, iy, 44, 62});
    g.text(c.name, ix + 108, iy + 4, 24, theme::text, true);
    std::string kind = c.type == ClassType::Pair ? "pocket pair" : c.type == ClassType::Suited ? "suited" : "offsuit";
    g.text(kind + "  \xC2\xB7  " + std::to_string(c.combos) + " combos", ix + 108, iy + 36, 13, theme::muted);
    iy += 80;
    g.text("Rank #" + std::to_string(D.rankPos[selected_] + 1) + " of 169  \xC2\xB7  top " + pct(D.percentile[selected_]) + " of hands", ix, iy, 14, theme::text2);
    iy += 30;
    const HandEq& e = handEquity(selected_);
    auto eqBar = [&](const std::string& label, double v) {
      g.text(label, ix, iy, 13, theme::text2);
      g.textRight(pct(v, 1), ix + iw, iy, 13, theme::text, true);
      Rectangle b{ix, iy + 20, iw, 8};
      DrawRectangleRounded(b, 1, 6, theme::surface3);
      DrawRectangleRounded({b.x, b.y, static_cast<float>(b.width * v), b.height}, 1, 6, COL_CALL);
      iy += 40;
    };
    eqBar("Equity vs a random hand", e.vsRandom);
    eqBar("Equity vs a tight range (top 10%)", e.vsTop10);
    std::string opens;
    for (auto& p : positions)
      if (rfiSet(p)->test(selected_)) opens += (opens.empty() ? "" : ", ") + (p == "HU" ? std::string("HU button") : p);
    g.text("Open-raise from", ix, iy, 13, theme::muted, true);
    iy += 20;
    iy += g.textWrapped(opens.empty() ? std::string("never - fold when first in") : opens, ix, iy, iw, 14, theme::text) + 14;
    if (mode_ == 1 && mix) {
      const auto& w = (*mix)[selected_];
      g.text(std::string("Facing a ") + OPEN_POS[openerPos_] + " raise in the " + HERO_POS[heroPos_], ix, iy, 13, theme::muted, true);
      iy += 22;
      Rectangle b{ix, iy, iw, 14};
      float sx = b.x;
      Color cols[3] = {COL_RAISE, COL_CALL, COL_FOLD};
      for (int k = 0; k < 3; ++k) {
        float sw = static_cast<float>(b.width * w[k]);
        if (sw >= 1) DrawRectangleRec({sx, b.y, std::max(1.f, sw - 2), b.height}, cols[k]);
        sx += sw;
      }
      iy += 20;
      g.text("3-bet " + pct(w[0]) + "   call " + pct(w[1]) + "   fold " + pct(w[2]), ix, iy, 13, theme::text2);
      iy += 30;
    }
  } else {
    g.text("Click a hand in the grid", ix, iy, 15, theme::text, true);
    iy += 26;
    iy += g.textWrapped("You will see its rank, how often it wins against different ranges, and how to play it from each seat.", ix, iy, iw, 13.5f, theme::muted) + 20;
  }
  g.text("How to use this", ix, iy, 16, theme::text, true);
  iy += 28;
  const char* tips[] = {"Open tighter in early position: more players are left to act behind you.", "The button opens the widest - you are guaranteed position after the flop.",
                        "Facing a raise, continue with fewer hands than the raiser opens; 3-bet the best ones.", "The big blind defends widest because it already has 1bb in the pot.",
                        "Drill these in Drills > Preflop decisions."};
  for (const char* t : tips) {
    if (iy > ip.y + ip.height - 40) break;
    g.text("\xE2\x80\xA2", ix, iy, 13.5f, theme::muted);
    iy += g.textWrapped(t, ix + 14, iy, iw - 14, 13.5f, theme::text2) + 8;
  }
}

}  // namespace hl::ui
