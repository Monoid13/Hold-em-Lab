#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "bots/profiles.hpp"
#include "core/evaluator.hpp"
#include "core/handinfo.hpp"
#include "ui/app.hpp"

namespace hl::ui {

namespace {
std::string hudLine(const StatsBook& s, const std::string& id) {
  const PlayerStats& p = s.peek(id);
  if (!p.hands) return "no reads yet";
  auto v = [&](const char* k) {
    auto r = s.raw(id, k);
    return r.pct ? std::to_string(static_cast<int>(std::lround(*r.pct * 100))) : std::string("-");
  };
  auto af = s.af(id);
  char afs[16];
  if (!af) std::snprintf(afs, sizeof afs, "-");
  else if (std::isinf(*af)) std::snprintf(afs, sizeof afs, "inf");
  else std::snprintf(afs, sizeof afs, "%.1f", *af);
  return v("vpip") + "/" + v("pfr") + "/" + afs + " \xC2\xB7 " + std::to_string(p.hands) + "h";
}

void bubble(Gui& g, const std::string& text, Vector2 anchorTop, Color fill, Color ink, float size = 13) {
  Vector2 m = g.measure(text, size, true);
  Rectangle r{anchorTop.x - m.x / 2 - 9, anchorTop.y, m.x + 18, m.y + 6};
  DrawRectangleRounded(r, 1, 10, fill);
  g.textCentered(text, r, size, ink, true);
}
}  // namespace

std::string PlayScreen::decisionKey(App& app) const {
  const auto& t = app.ctrl.table;
  if (!t || !t->hasHand) return "";
  return std::to_string(t->hand.id) + ":" + std::to_string(t->hand.log.size());
}

void PlayScreen::perform(App& app, const std::string& act) {
  GameController& c = app.ctrl;
  if (act == "deal") {
    if (c.st.handOver) c.deal();
    return;
  }
  if (act == "pause") {
    c.togglePause();
    return;
  }
  if (act == "hint") {
    if (c.st.awaitingHero) {
      hintShown_ = true;
      hintKey_ = decisionKey(app);
    }
    return;
  }
  if (!c.st.awaitingHero || !c.table) return;
  Legal l = c.table->legal();
  if (act == "fold") {
    if (l.canCheck) return;  // never fold when checking is free
    c.heroAct({Act::Fold, 0});
  } else if (act == "call")
    c.heroAct({l.toCall > 0 ? Act::Call : Act::Check, 0});
  else if (act == "raise") {
    if (!(l.canBet || l.canRaise)) return;
    c.heroAct({l.canBet ? Act::Bet : Act::Raise, std::clamp(amount_, l.minTo, l.maxTo)});
  }
}

void PlayScreen::handleKeys(App& app) {
  if (IsKeyPressed(KEY_F)) perform(app, "fold");
  if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_K)) perform(app, "call");
  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_B)) perform(app, "raise");
  if (IsKeyPressed(KEY_H)) perform(app, "hint");
  if (IsKeyPressed(KEY_P)) perform(app, "pause");
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) perform(app, "deal");
  if (app.ctrl.st.awaitingHero && app.ctrl.table) {
    Legal l = app.ctrl.table->legal();
    int step = std::max(1, app.ctrl.table->hand.bb);
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) amount_ = std::min(l.maxTo, amount_ + step);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) amount_ = std::max(l.minTo, amount_ - step);
  }
}

void PlayScreen::draw(App& app, Rectangle area) {
  float sideW = area.width >= 1180 ? 350 : 300;
  Rectangle tableCol{area.x, area.y, area.width - sideW - 14, area.height};
  float barH = 128;
  drawTable(app, {tableCol.x, tableCol.y, tableCol.width, tableCol.height - barH - 10});
  drawActionBar(app, {tableCol.x, tableCol.y + tableCol.height - barH, tableCol.width, barH});
  drawSidebar(app, {area.x + area.width - sideW, area.y, sideW, area.height});
}

void PlayScreen::drawTable(App& app, Rectangle r) {
  Gui& g = app.g;
  GameController& c = app.ctrl;
  const Settings& set = app.settings;
  if (!c.table) return;
  Table& t = *c.table;
  const bool has = t.hasHand;
  const Hand& h = t.hand;
  const GameState& st = c.st;
  const bool runningOut = has && h.finished && st.revealCount < 5;
  const bool finishedShown = has && h.finished && !runningOut && st.handOver;

  float cx = r.x + r.width / 2, cy = r.y + r.height / 2 + 4;
  float rx = r.width * 0.40f, ry = r.height * 0.34f;
  // felt
  DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 10), rx + 22, ry + 22, Color{0, 0, 0, 90});
  DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rx + 16, ry + 16, theme::rail);
  DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rx, ry, theme::feltDark);
  for (int i = 0; i < 24; ++i) {  // soft radial highlight
    float k = 1 - i * 0.035f;
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - ry * 0.1f * i / 24), rx * k, ry * k, withAlpha(Color{31, 122, 79, 255}, 0.07f));
  }

  std::set<Card> bestCards;
  std::map<int, const ShowdownHand*> sd;
  if (has && h.finished && !h.results.uncontested)
    for (auto& s : h.results.showdown) {
      sd[s.seat] = &s;
      if (finishedShown) {
        auto it = h.results.winnings.find(s.seat);
        if (it != h.results.winnings.end() && it->second > 0)
          for (Card cc : s.bestCards) bestCards.insert(cc);
      }
    }

  const int n = t.numSeats;
  float cardW = std::clamp(r.height * 0.075f, 30.f, 54.f), cardH = cardW * 1.4f;
  float boxW = std::clamp(r.width * 0.15f, 120.f, 170.f), boxH = 50;
  int potTotal = 0;
  for (int i = 0; i < n; ++i) {
    const SeatPlayer& p = t.seats[i];
    if (!p.occupied) continue;
    float a = (90.f + i * 360.f / n) * DEG2RAD;
    float ca = std::cos(a), sa = std::sin(a);
    Vector2 pos{cx + (rx + 26) * 1.04f * ca, cy + (ry + 30) * 1.0f * sa};
    pos.y = std::clamp(pos.y, r.y + cardH + 4, r.y + r.height - boxH - 30);
    pos.x = std::clamp(pos.x, r.x + boxW / 2 + 2, r.x + r.width - boxW / 2 - 2);
    const HandPlayer* hp = has && h.p(i).inHand ? &h.p(i) : nullptr;
    bool isHero = p.isHero;
    int stack = hp ? hp->stack : p.stack;
    int bet = hp ? hp->bet : 0;
    if (runningOut && hp) {
      stack = hp->startStack - hp->contrib;
      bet = 0;
    }
    if (hp) potTotal += runningOut ? hp->contrib : bet;
    bool folded = !hp || hp->folded;
    Rectangle box{pos.x - boxW / 2, pos.y - boxH / 2, boxW, boxH};

    // cards
    if (hp) {
      bool reveal = isHero || set.showAllCards || ((runningOut || finishedShown) && sd.count(i));
      if (!hp->folded || isHero || set.showAllCards) {
        float total = cardW * 2 + 4;
        for (int k = 0; k < 2; ++k) {
          Rectangle cr{pos.x - total / 2 + k * (cardW + 4), box.y - cardH + 12, cardW, cardH};
          g.card(hp->hole[k], cr, !reveal, finishedShown && bestCards.count(hp->hole[k]), hp->folded);
        }
      }
    }
    // box
    bool acting = has && !h.finished && h.actor == i;
    bool winner = false;
    if (finishedShown) {
      auto it = h.results.winnings.find(i);
      winner = it != h.results.winnings.end() && it->second > 0;
    }
    Color fill = isHero ? Color{28, 36, 32, 245} : Color{15, 20, 18, 235};
    DrawRectangleRounded(box, 0.3f, 8, folded ? withAlpha(fill, 0.6f) : fill);
    Color line = acting ? theme::accent : winner ? theme::gain : theme::border;
    DrawRectangleRoundedLinesEx(box, 0.3f, 8, acting || winner ? 2.f : 1.f, line);
    unsigned col = isHero ? 0xe6b450 : (c.bots.count(i) ? c.bots[i].color : 0x888888);
    Vector2 av{box.x + 24, pos.y};
    DrawCircleV(av, 15, folded ? withAlpha(hexColor(col), 0.45f) : hexColor(col));
    g.textCentered(isHero ? "You" : p.name.substr(0, 1), {av.x - 15, av.y - 15, 30, 30}, isHero ? 11 : 15, theme::accentInk, true);
    Color ink = folded ? theme::muted : theme::text;
    std::string posLabel = hp ? hp->position : "";
    float nameMax = boxW - 52 - (posLabel.empty() ? 0 : g.measure(posLabel, 11, true).x + 12) - (st.thinkingSeat == i ? 30 : 0);
    std::string nm = g.ellipsize(p.name, nameMax, 14, true);
    g.text(nm, box.x + 46, box.y + 7, 14, ink, true);
    if (!posLabel.empty()) {
      float px = box.x + 50 + g.measure(nm, 14, true).x;
      Rectangle pr{px, box.y + 8, g.measure(posLabel, 11, true).x + 8, 16};
      DrawRectangleRounded(pr, 0.4f, 6, theme::surface3);
      g.textCentered(posLabel, pr, 11, theme::text2, true);
    }
    g.text(p.stack <= 0 && !hp ? "busted" : fmtInt(stack), box.x + 46, box.y + 27, 14, folded ? withAlpha(theme::accent, 0.5f) : theme::accent);
    if (st.thinkingSeat == i) {
      for (int k = 0; k < 3; ++k) {
        float ph = static_cast<float>(std::fmod(GetTime() * 2.5 + k * 0.33, 1.0));
        DrawCircleV({box.x + box.width - 26 + k * 8.f, box.y + 15}, 2.6f, withAlpha(theme::accent, 0.3f + 0.7f * std::sin(ph * PI)));
      }
    }
    float below = box.y + box.height + 2;
    if (!isHero && set.showOppStats) {
      std::string hl = hudLine(app.stats, p.id);
      Vector2 m = g.measure(hl, 11);
      Rectangle hr{pos.x - m.x / 2 - 7, below, m.x + 14, 16};
      DrawRectangleRounded(hr, 0.5f, 6, Color{0, 0, 0, 140});
      g.textCentered(hl, hr, 11, theme::text2);
      if (g.hover(box)) g.setTooltip("VPIP / PFR / aggression factor \xC2\xB7 hands observed");
      below += 17;
    }
    if (!isHero && set.revealProfiles && c.bots.count(i)) {
      std::string lab = profile(c.bots[i].profileKey).label + (c.bots[i].memory.tilt > 0.35 ? " (tilted)" : "");
      g.textCentered(lab, {pos.x - 80, below, 160, 15}, 11, Color{255, 217, 138, 255});
      below += 15;
    }
    if (finishedShown && sd.count(i)) {
      bubble(g, sd[i]->handName, {pos.x, below + 1}, Color{0, 0, 0, 190}, theme::text, 12);
      below += 22;
    }
    if (finishedShown && winner) {
      auto it = h.results.winnings.find(i);
      std::string w = "+" + fmtInt(it->second);
      Vector2 m = g.measure(w, 15, true);
      g.text(w, pos.x - m.x / 2, below, 15, theme::gain, true);
    }
    auto eq = st.allInEquity.find(i);
    if (eq != st.allInEquity.end() && set.allInEquity && (runningOut || finishedShown)) {
      std::string e = pct(eq->second);
      Vector2 m = g.measure(e, 13, true);
      Rectangle er{box.x + box.width - m.x - 6, box.y - 12, m.x + 14, 20};
      DrawRectangleRounded(er, 1, 8, Color{11, 42, 90, 255});
      DrawRectangleRoundedLinesEx(er, 1, 8, 1, theme::series);
      g.textCentered(e, er, 13, WHITE, true);
    }
    auto fl = st.flashes.find(i);
    if (fl != st.flashes.end() && !finishedShown) {
      std::string f = fl->second;
      auto paren = f.find(" (");
      if (paren != std::string::npos) f = f.substr(0, paren);  // "Bet 40 (1/2 pot)" -> "Bet 40"
      Vector2 m = g.measure(f, 11.5f, true);
      float stackEnd = box.x + 46 + g.measure(fmtInt(stack), 14).x + 8;
      Rectangle pill{std::max(stackEnd, box.x + box.width - m.x - 22), box.y + 26, m.x + 14, 18};
      DrawRectangleRounded(pill, 1, 8, Color{243, 241, 234, 255});
      g.textCentered(f, pill, 11.5f, Color{26, 26, 26, 255}, true);
    }
    auto chat = st.chats.find(i);
    if (chat != st.chats.end()) bubble(g, chat->second.text, {pos.x, box.y - cardH - 6}, WHITE, Color{34, 34, 34, 255}, 12.5f);
    if (bet > 0) {
      Vector2 bp{cx + rx * 0.70f * ca, cy + ry * 0.62f * sa};
      std::string bt = fmtInt(bet);
      Vector2 m = g.measure(bt, 13, true);
      Rectangle br{bp.x - (m.x + 30) / 2, bp.y - 11, m.x + 30, 22};
      DrawRectangleRounded(br, 1, 8, Color{0, 0, 0, 140});
      g.chip({br.x + 11, bp.y}, 7);
      g.text(bt, br.x + 22, bp.y - m.y / 2, 13, WHITE, true);
    }
    if (has && h.buttonSeat == i) {
      Vector2 dp{cx + rx * 0.84f * std::cos(a - 0.3f), cy + ry * 0.80f * std::sin(a - 0.3f)};
      DrawCircleV(dp, 11, Color{247, 244, 234, 255});
      g.textCentered("D", {dp.x - 11, dp.y - 11, 22, 22}, 12, Color{34, 34, 34, 255}, true);
    }
  }

  // board + pot
  float bw = std::clamp(r.height * 0.09f, 36.f, 64.f), bh = bw * 1.4f;
  float total = bw * 5 + 8 * 4;
  float bx = cx - total / 2, by = cy - bh / 2 - 12;
  int count = has ? (runningOut ? st.revealCount : static_cast<int>(h.board.size())) : 0;
  for (int k = 0; k < 5; ++k) {
    Rectangle cr{bx + k * (bw + 8), by, bw, bh};
    if (k < count) g.card(h.board[k], cr, false, finishedShown && bestCards.count(h.board[k]));
    else g.cardSlot(cr);
  }
  int potShown = has && !h.finished ? t.potTotal() : (runningOut ? potTotal : 0);
  if (potShown > 0) {
    std::string pt = "Pot " + fmtInt(potShown);
    Vector2 m = g.measure(pt, 15, true);
    Rectangle pr{cx - m.x / 2 - 14, by + bh + 10, m.x + 28, 26};
    DrawRectangleRounded(pr, 1, 8, Color{0, 0, 0, 80});
    g.textCentered(pt, pr, 15, Color{230, 242, 234, 255}, true);
  }
  std::string msg;
  if (finishedShown) {
    for (auto& [seat, amt] : h.results.winnings) {
      if (!msg.empty()) msg += "  \xC2\xB7  ";
      msg += (h.p(seat).id == HERO_ID ? std::string("You win ") : h.p(seat).name + " wins ") + fmtInt(amt);
    }
    if (!h.results.uncontested)
      for (auto& s : h.results.showdown) {
        auto it = h.results.winnings.find(s.seat);
        if (it != h.results.winnings.end() && it->second > 0) {
          msg += " - " + s.handName;
          break;
        }
      }
  } else if (st.levelUp && has && h.log.size() <= 3)
    msg = "Blinds up: " + std::to_string(h.sb) + "/" + std::to_string(h.bb) + (h.ante ? " (ante " + std::to_string(h.ante) + ")" : "");
  if (!msg.empty()) {
    Vector2 m = g.measure(msg, 15, true);
    g.text(msg, cx - m.x / 2 + 1, by + bh + 45, 15, Color{0, 0, 0, 160}, true);
    g.text(msg, cx - m.x / 2, by + bh + 44, 15, Color{255, 244, 214, 255}, true);
  }
  if (c.paused()) {
    Rectangle pr{cx - 60, by - 44, 120, 28};
    DrawRectangleRounded(pr, 1, 8, Color{0, 0, 0, 150});
    g.textCentered("Paused", pr, 15, theme::accent, true);
  }
  if (st.gameOver.active) {
    DrawRectangleRec(r, Color{0, 0, 0, 140});
    Rectangle box{cx - 220, cy - 80, 440, 160};
    g.panel(box);
    std::string title = st.gameOver.won ? "You won the tournament!" : "You finished " + std::to_string(st.gameOver.place) + " of " + std::to_string(st.gameOver.of);
    g.textCentered(title, {box.x, box.y + 24, box.width, 30}, 22, theme::text, true);
    if (g.button({cx - 80, box.y + 96, 160, 40}, "Play again", Btn::Primary)) app.startNewGame();
  }
}

void PlayScreen::drawActionBar(App& app, Rectangle r) {
  Gui& g = app.g;
  GameController& c = app.ctrl;
  g.panel(r);
  if (!c.table) return;
  float x = r.x + 14, y = r.y + 12;
  if (!c.st.awaitingHero) {
    float cy = r.y + r.height / 2 - 20;
    if (c.st.heroBusted) {
      g.text("You are out of chips.", x, cy + 10, 15, theme::text2);
      if (g.button({x + 180, cy, 200, 40}, "Rebuy (" + fmtInt(c.startingStack) + ")", Btn::Primary)) c.rebuyHero();
    } else if (c.st.gameOver.active)
      g.text("Game over.", x, cy + 10, 15, theme::text2);
    else if (c.st.handOver) {
      if (g.button({x, cy, 230, 42}, "Deal next hand  [Space]", Btn::Primary)) c.deal();
    } else if (c.st.thinkingSeat >= 0)
      g.text(c.table->seats[c.st.thinkingSeat].name + " is thinking...", x, cy + 10, 15, theme::muted);
    if (c.table->hasHand && !c.table->hand.finished && c.table->hand.p(c.heroSeat).inHand && c.table->hand.p(c.heroSeat).folded)
      g.text("You folded - watch how the hand plays out.", x + 300, cy + 10, 14, theme::muted);
    float rx = r.x + r.width - 14;
    if (g.button({rx - 110, cy, 110, 40}, c.paused() ? "Resume [P]" : "Pause [P]", Btn::Ghost, true, 14)) c.togglePause();
    bool ad = app.settings.autoDeal;
    if (g.toggle({rx - 240, cy + 9, 120, 24}, "Auto-deal", ad)) {
      app.settings.autoDeal = ad;
      app.settings.save();
      if (ad && c.st.handOver) c.deal();
    }
    return;
  }
  Legal l = c.table->legal();
  std::string key = decisionKey(app);
  if (key != amountKey_ || amount_ < l.minTo || amount_ > l.maxTo) {
    amountKey_ = key;
    const Hand& h = c.table->hand;
    if (h.street == Street::Preflop) amount_ = h.raiseCount == 0 ? static_cast<int>(std::lround(2.5 * h.bb)) : l.currentBet * 3;
    else if (l.canBet) amount_ = l.myBet + static_cast<int>(std::lround(0.5 * l.pot));
    else amount_ = static_cast<int>(std::lround(l.currentBet * 2.5));
    amount_ = std::clamp(amount_, l.minTo, l.maxTo);
  }
  // info row
  std::string info = "Pot " + fmtInt(l.pot);
  if (l.toCall > 0) info += "     To call " + fmtInt(l.toCall) + "     Need " + pct(static_cast<double>(l.toCall) / (l.pot + l.toCall)) + " equity";
  else info += "     No bet to you";
  info += "     Stack " + fmtInt(l.stack);
  g.text(info, x, y + 2, 14, theme::text2);
  if (g.button({r.x + r.width - 120, y - 3, 106, 28}, "Hint [H]", Btn::Ghost, true, 13)) perform(app, "hint");
  // main buttons
  y += 30;
  bool canAgg = l.canBet || l.canRaise;
  float bwid = (r.width - 28 - 16) / 3;
  float bh = 42;
  if (g.button({x, y, bwid, bh}, "Fold  [F]", Btn::Fold, !l.canCheck, 16)) perform(app, "fold");
  std::string callLabel = l.toCall > 0 ? (l.toCall >= l.stack ? "Call all-in " : "Call ") + fmtInt(l.toCall) + "  [C]" : "Check  [C]";
  if (g.button({x + bwid + 8, y, bwid, bh}, callLabel, Btn::Call, true, 16)) perform(app, "call");
  if (canAgg) {
    std::string rl = amount_ >= l.maxTo ? "All-in " + fmtInt(l.maxTo) : (l.canBet ? "Bet " : "Raise to ") + fmtInt(amount_);
    if (g.button({x + 2 * (bwid + 8), y, bwid, bh}, rl + "  [R]", Btn::Raise, true, 16)) perform(app, "raise");
  }
  // sizing
  y += bh + 8;
  if (canAgg) {
    const Hand& h = c.table->hand;
    std::vector<std::pair<std::string, int>> presets;
    if (h.street == Street::Preflop) {
      if (h.raiseCount == 0)
        for (double m : {2.5, 3.0, 4.0}) presets.push_back({(m == 2.5 ? std::string("2.5") : std::to_string(static_cast<int>(m))) + "bb", static_cast<int>(std::lround(m * h.bb))});
      else {
        presets.push_back({"3x", l.currentBet * 3});
        presets.push_back({"4x", l.currentBet * 4});
      }
      presets.push_back({"Pot", l.currentBet + l.pot + l.toCall});
    } else if (l.canBet) {
      presets = {{"1/3", l.myBet + static_cast<int>(std::lround(0.33 * l.pot))}, {"1/2", l.myBet + static_cast<int>(std::lround(0.5 * l.pot))},
                 {"2/3", l.myBet + static_cast<int>(std::lround(0.66 * l.pot))}, {"Pot", l.myBet + l.pot}};
    } else {
      presets = {{"2.5x", static_cast<int>(std::lround(l.currentBet * 2.5))}, {"Pot", l.currentBet + l.pot + l.toCall}};
    }
    presets.push_back({"All-in", l.maxTo});
    float px = x;
    for (auto& [lab, v] : presets) {
      int val = std::clamp(v, l.minTo, l.maxTo);
      float w = g.measure(lab, 13, true).x + 22;
      if (g.button({px, y, w, 30}, lab, amount_ == val ? Btn::ChipOn : Btn::Chip, true, 13)) amount_ = val;
      px += w + 6;
    }
    float amtW = 90;
    float sx = px + 10, sw = r.x + r.width - 14 - amtW - 80 - sx;
    if (sw > 60) g.slider({sx, y, sw, 30}, amount_, l.minTo, l.maxTo, std::max(1, h.bb / 2));
    float ax = r.x + r.width - 14 - amtW - 74;
    int step = std::max(1, h.bb);
    if (g.button({ax, y, 34, 30}, "\xE2\x88\x92", Btn::Normal, amount_ > l.minTo, 16)) amount_ = std::max(l.minTo, amount_ - step);
    Rectangle ar{ax + 38, y, amtW, 30};
    DrawRectangleRounded(ar, 0.3f, 6, theme::surface2);
    g.textCentered(fmtInt(amount_), ar, 15, theme::text, true);
    if (g.button({ax + 42 + amtW, y, 34, 30}, "+", Btn::Normal, amount_ < l.maxTo, 16)) amount_ = std::min(l.maxTo, amount_ + step);
  } else
    g.text("No raise possible (everyone else is all-in or the action is not re-opened).", x, y + 6, 13, theme::muted);
}

void PlayScreen::drawSidebar(App& app, Rectangle r) {
  Gui& g = app.g;
  GameController& c = app.ctrl;
  const Settings& set = app.settings;
  if (hintKey_ != decisionKey(app)) hintShown_ = false;
  float y0 = sideScroll_.begin(g, r);
  float y = y0;
  const float w = r.width - 8;
  const float pad = 14;

  // --- HUD ---
  if (set.showHud && c.table && c.table->hasHand && c.table->hand.p(c.heroSeat).inHand) {
    const Hand& h = c.table->hand;
    const HandPlayer& hp = h.p(c.heroSeat);
    int nb = static_cast<int>(h.board.size());
    if (h.finished && c.st.revealCount < 5) nb = c.st.revealCount;
    std::vector<Card> board(h.board.begin(), h.board.begin() + nb);
    HandAnalysis info = analyzeHand({hp.hole[0], hp.hole[1]}, board);
    const std::optional<Spot>& spot = c.st.spot;
    int cardsToCome = nb == 3 ? 2 : nb == 4 ? 1 : 0;
    std::vector<std::pair<std::string, std::string>> rows;
    rows.push_back({"Hand", info.name + (info.detail.empty() ? "" : " \xC2\xB7 " + info.detail)});
    if (!info.draws.description.empty()) rows.push_back({"Draws", info.draws.text()});
    if (cardsToCome && info.outs.drawOuts)
      rows.push_back({"Outs", std::to_string(info.outs.drawOuts) + " = " + pct(outsToEquity(info.outs.drawOuts, 1)) + " next card" +
                                  (cardsToCome == 2 ? ", " + pct(outsToEquity(info.outs.drawOuts, 2)) + " by river" : "")});
    if (info.texture.valid) rows.push_back({"Board", info.texture.description});
    rows.push_back({"Position", hp.position + (spot ? (spot->ip ? " \xC2\xB7 in position" : " \xC2\xB7 out of position") : "")});
    if (spot && !hp.folded && !spot->oppRanges.empty()) {
      std::string rs;
      for (auto& o : spot->oppRanges) rs += (rs.empty() ? "" : " \xC2\xB7 ") + o.name + " ~" + pct(o.width);
      rows.push_back({"Ranges", rs});
    }
    float vw = w - pad * 2 - 74;
    float hh = 44;
    bool showEq = spot && !hp.folded;
    if (showEq) hh += 46;
    for (auto& [k, v] : rows) hh += g.wrappedHeight(v, vw, 13.5f) + 4;
    bool hintBtn = c.st.awaitingHero && spot && !hintShown_;
    if (hintBtn) hh += 42;
    float hintH = 0;
    std::vector<std::string> hintLines;
    if (hintShown_ && spot) {
      hintLines.push_back("Coach suggests: " + spot->recommendedLabel);
      if (spot->isPreflop) {
        std::string mix;
        for (auto& [lab, wgt] : spot->mix) mix += (mix.empty() ? "" : " \xC2\xB7 ") + lab + " " + pct(wgt);
        hintLines.push_back(mix);
        for (auto& nline : spot->preflop.notes) hintLines.push_back("\xE2\x80\xA2 " + nline);
      } else {
        auto opts = spot->postflop.options;
        std::sort(opts.begin(), opts.end(), [](const Option& a, const Option& b) { return a.ev > b.ev; });
        for (size_t i = 0; i < opts.size() && i < 4; ++i) {
          std::string l = actionLabel(opts[i].type, opts[i].amount, spot->view) + ":  EV " + fmtChips(opts[i].ev);
          if (opts[i].foldAll >= 0) l += "  \xC2\xB7  folds " + pct(opts[i].foldAll);
          hintLines.push_back(l);
        }
        hintLines.push_back("EV in chips relative to folding now.");
      }
      for (auto& l : hintLines) hintH += g.wrappedHeight(l, w - pad * 2 - 16, 13) + 3;
      hh += hintH + 24;
    }
    Rectangle pr{r.x, y, w, hh};
    g.panel(pr, "Your hand");
    g.textRight(cardsPretty({hp.hole[0], hp.hole[1]}), pr.x + pr.width - pad, pr.y + 12, 16, theme::text, true);
    float yy = pr.y + 42;
    if (showEq) {
      double eq = spot->equity, need = spot->needed;
      Rectangle bar{pr.x + pad, yy, pr.width - pad * 2, 12};
      DrawRectangleRounded(bar, 1, 6, theme::surface3);
      DrawRectangleRounded({bar.x, bar.y, static_cast<float>(bar.width * eq), bar.height}, 1, 6, theme::series);
      if (need > 0) DrawRectangle(static_cast<int>(bar.x + bar.width * need) - 1, static_cast<int>(bar.y - 4), 2, 20, theme::text);
      std::string leg = "Equity " + pct(eq);
      if (need > 0) leg += "     Need " + pct(need);
      char spr[32];
      std::snprintf(spr, sizeof spr, "     SPR %.1f", spot->spr);
      g.text(leg + spr, bar.x, yy + 18, 13, theme::text2);
      if (g.hover({bar.x, bar.y - 4, bar.width, 40})) g.setTooltip("Equity vs the opponents' estimated ranges (built only from their actions and stats). The white mark is the equity you need to call.");
      yy += 46;
    }
    for (auto& [k, v] : rows) {
      g.text(k, pr.x + pad, yy, 13.5f, theme::muted);
      yy += g.textWrapped(v, pr.x + pad + 74, yy, vw, 13.5f, theme::text) + 4;
    }
    if (hintBtn && g.button({pr.x + pad, yy + 6, pr.width - pad * 2, 30}, "Show coach hint  [H]", Btn::Ghost, true, 13)) perform(app, "hint");
    if (hintShown_ && spot) {
      Rectangle hr{pr.x + pad, yy + 8, pr.width - pad * 2, hintH + 14};
      DrawRectangleRounded(hr, 0.1f, 6, theme::surface2);
      float hy = hr.y + 7;
      for (size_t i = 0; i < hintLines.size(); ++i) hy += g.textWrapped(hintLines[i], hr.x + 8, hy, hr.width - 16, 13, i == 0 ? theme::accent : theme::text2, i == 0) + 3;
    }
    y += hh + 12;
  }

  // --- Coach ---
  if (set.coachMode != "off") {
    Totals T = app.progress.totals();
    std::string sub = T.accuracy >= 0 ? "Decision score " + pct(T.accuracy) + " \xC2\xB7 " + std::to_string(T.decisions) + " graded" : "";
    const auto& lg = c.st.lastGrade;
    bool show = lg && (set.coachMode == "all" || (lg->grade != GradeLevel::Best && lg->grade != GradeLevel::Good));
    float hh = 44;
    std::vector<std::string> lines;
    std::string who;
    if (show) {
      who = "You: " + lg->heroLabel + (lg->grade != GradeLevel::Best ? "  \xC2\xB7  Coach: " + lg->bestLabel : "");
      lines = lg->lines;
      hh += 34 + g.wrappedHeight(who, w - pad * 2, 13.5f, true) + 4;
      for (auto& l : lines) hh += g.wrappedHeight(l, w - pad * 2 - 12, 13) + 4;
    } else
      hh += g.wrappedHeight("Make a decision and the coach will grade it against the estimated EV of every option.", w - pad * 2, 13) + 4;
    Rectangle pr{r.x, y, w, hh};
    g.panel(pr, "Coach", sub);
    float yy = pr.y + 42;
    if (show) {
      g.gradeBadge(pr.x + pad, yy, static_cast<int>(lg->grade));
      float bx = pr.x + pad + g.gradeBadgeWidth(static_cast<int>(lg->grade)) + 8;
      g.text(streetName(lg->street), bx, yy + 4, 13, theme::muted);
      if (lg->hasEvLoss && lg->grade != GradeLevel::Best && lg->evLossBB > 0.05) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "-%.1f bb EV", lg->evLossBB);
        g.textRight(buf, pr.x + pr.width - pad, yy + 4, 13, theme::text2);
      }
      yy += 32;
      yy += g.textWrapped(who, pr.x + pad, yy, w - pad * 2, 13.5f, theme::text, true) + 4;
      for (auto& l : lines) {
        g.text("\xE2\x80\xA2", pr.x + pad, yy, 13, theme::muted);
        yy += g.textWrapped(l, pr.x + pad + 12, yy, w - pad * 2 - 12, 13, theme::text2) + 4;
      }
    } else
      g.textWrapped("Make a decision and the coach will grade it against the estimated EV of every option.", pr.x + pad, yy, w - pad * 2, 13, theme::muted);
    y += hh + 12;
  }

  // --- Log ---
  float logH = 230;
  Rectangle lr{r.x, y, w, logH};
  g.panel(lr, "Hand log");
  Rectangle inner{lr.x + pad, lr.y + 40, lr.width - pad * 2 + 6, lr.height - 48};
  const auto& log = c.st.log;
  if (log.size() != lastLogSize_) {  // new lines: stick to the bottom
    lastLogSize_ = log.size();
    float total = 0;
    for (auto& l : log) total += g.wrappedHeight(l, inner.width - 12, 13) + 2;
    logScroll_.offset = std::max(0.f, total - inner.height);
  }
  float ly = logScroll_.begin(g, inner);
  float lh = 0;
  for (auto& l : log) lh += g.textWrapped(l, inner.x, ly + lh, inner.width - 12, 13, theme::text2) + 2;
  logScroll_.end(g, lh);
  y += logH + 12;
  sideScroll_.end(g, y - y0);
}

}  // namespace hl::ui
