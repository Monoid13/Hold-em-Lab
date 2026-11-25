#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "bots/profiles.hpp"
#include "ui/app.hpp"

namespace hl::ui {

void ReviewScreen::handleKeys(App& app) {
  const HandRecord* rec = nullptr;
  for (auto& h : app.history.hands)
    if (h.id == selected_) rec = &h;
  if (!rec) return;
  int maxStep = static_cast<int>(rec->steps.size()) - 1;
  if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) step_ = std::min(maxStep, step_ + 1);
  if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) step_ = std::max(0, step_ - 1);
  if (IsKeyPressed(KEY_HOME)) step_ = 0;
  if (IsKeyPressed(KEY_END)) step_ = maxStep;
}

void ReviewScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  auto& hands = app.history.hands;
  // filters
  std::vector<const HandRecord*> list;
  for (auto it = hands.rbegin(); it != hands.rend(); ++it) {
    const HandRecord& h = *it;
    bool keep = true;
    if (filter == 1) {
      keep = false;
      for (auto& d : h.decisions)
        if (static_cast<int>(d.grade) >= 2) keep = true;
    } else if (filter == 2)
      keep = std::abs(h.heroNet()) >= 20 * h.bb;
    else if (filter == 3)
      keep = !h.uncontested;
    if (keep) list.push_back(&h);
  }
  const HandRecord* rec = nullptr;
  for (auto* h : list)
    if (h->id == selected_) rec = h;
  if (!rec && !list.empty()) {
    rec = list.front();
    selected_ = rec->id;
    step_ = 0;
  }

  float listW = std::min(360.f, area.width * 0.32f);
  Rectangle lp{area.x, area.y, listW, area.height};
  g.panel(lp);
  static const char* fl[] = {"All", "With mistakes", "Big pots", "Showdowns"};
  float fx = lp.x + 10, fy = lp.y + 10;
  for (int i = 0; i < 4; ++i) {
    float w = g.measure(fl[i], 12.5f, true).x + 20;
    if (fx + w > lp.x + lp.width - 10) {
      fx = lp.x + 10;
      fy += 32;
    }
    if (g.button({fx, fy, w, 26}, fl[i], filter == i ? Btn::ChipOn : Btn::Chip, true, 12.5f)) {
      filter = i;
      listScroll_.toTop();
    }
    fx += w + 6;
  }
  {  // totals for the hands in the current filter
    double net = 0;
    int won = 0, sd = 0, graded = 0, errors = 0;
    for (auto* h : list) {
      double n = static_cast<double>(h->heroNet()) / h->bb;
      net += n;
      won += n > 0;
      sd += !h->uncontested;
      for (auto& d : h->decisions) {
        ++graded;
        errors += static_cast<int>(d.grade) >= 2;
      }
    }
    float sy = fy + 34;
    Rectangle box{lp.x + 10, sy, lp.width - 20, 58};
    DrawRectangleRounded(box, 0.2f, 6, theme::surface2);
    float cw = box.width / 3;
    auto cell = [&](int i, const std::string& v, const std::string& l, Color c) {
      g.textCentered(v, {box.x + i * cw, box.y + 6, cw, 24}, 16, c, true);
      g.textCentered(l, {box.x + i * cw, box.y + 32, cw, 18}, 11.5f, theme::muted);
    };
    cell(0, std::to_string(list.size()), "hands", theme::text);
    cell(1, fmtBB(net), "net", net > 0 ? theme::gain : net < 0 ? theme::loss : theme::text);
    cell(2, graded ? std::to_string(errors) + "/" + std::to_string(graded) : "-", "errors / decisions", theme::text);
    if (g.hover(box)) g.setTooltip(std::to_string(won) + " hands won, " + std::to_string(sd) + " went to showdown.");
    fy += 66;
  }
  Rectangle inner{lp.x + 4, fy + 36, lp.width - 8, lp.y + lp.height - fy - 42};
  float y0 = listScroll_.begin(g, inner);
  float y = y0;
  if (list.empty()) g.textWrapped("No hands yet - play some at the table.", inner.x + 10, y + 10, inner.width - 20, 14, theme::muted);
  for (auto* h : list) {
    Rectangle row{inner.x, y, inner.width - 10, 44};
    bool on = h->id == selected_;
    if (on || g.hover(row)) DrawRectangleRounded(row, 0.2f, 6, on ? theme::surface3 : theme::surface2);
    if (g.clicked(row)) {
      selected_ = h->id;
      step_ = 0;
      detailScroll_.toTop();
    }
    g.text("#" + std::to_string(h->handNo), row.x + 8, row.y + 14, 13, theme::muted);
    const RecPlayer* hero = nullptr;
    for (auto& p : h->players)
      if (p.seat == h->heroSeat) hero = &p;
    if (hero) {
      g.card(hero->hole[0], {row.x + 54, row.y + 5, 24, 34});
      g.card(hero->hole[1], {row.x + 80, row.y + 5, 24, 34});
      g.text(hero->position, row.x + 114, row.y + 14, 13, theme::text2, true);
    }
    double net = static_cast<double>(h->heroNet()) / h->bb;
    g.textRight(fmtBB(net), row.x + row.width - 40, row.y + 14, 13.5f, net > 0 ? theme::gain : net < 0 ? theme::loss : theme::text2, true);
    bool any = false;
    GradeLevel wg = h->worstGrade(any);
    if (any) {
      static const char* sym[] = {"\xE2\x98\x85", "\xE2\x9C\x93", "?!", "?", "??"};
      Color cols[] = {theme::good, theme::good, theme::warning, theme::serious, theme::critical};
      Rectangle d{row.x + row.width - 32, row.y + 12, 26, 20};
      DrawRectangleRounded(d, 1, 8, cols[static_cast<int>(wg)]);
      g.textCentered(sym[static_cast<int>(wg)], d, 11, static_cast<int>(wg) == 2 || static_cast<int>(wg) == 3 ? Color{17, 17, 17, 255} : WHITE, true);
    }
    y += 48;
  }
  listScroll_.end(g, y - y0);

  // detail
  Rectangle dp{area.x + listW + 14, area.y, area.width - listW - 14, area.height};
  g.panel(dp);
  if (!rec) {
    g.text("Select a hand to replay it step by step.", dp.x + 20, dp.y + 20, 15, theme::muted);
    return;
  }
  step_ = std::clamp(step_, 0, static_cast<int>(rec->steps.size()) - 1);
  const Snapshot& sn = rec->steps[step_];
  Rectangle di{dp.x + 16, dp.y + 12, dp.width - 26, dp.height - 20};
  float dy0 = detailScroll_.begin(g, di);
  float dy = dy0;
  const float W = di.width - 14;
  g.text("Hand #" + std::to_string(rec->handNo), di.x, dy, 20, theme::text, true);
  std::time_t tt = static_cast<std::time_t>(rec->time / 1000);
  char tbuf[64];
  std::strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M", std::localtime(&tt));
  g.text(std::string(tbuf) + "  \xC2\xB7  blinds " + std::to_string(rec->sb) + "/" + std::to_string(rec->bb), di.x + g.measure("Hand #" + std::to_string(rec->handNo), 20, true).x + 14, dy + 5, 13, theme::muted);
  bool reveal = app.settings.revealInReview;
  if (g.toggle({di.x + W - 190, dy + 2, 190, 22}, "Show all hole cards", reveal)) {
    app.settings.revealInReview = reveal;
    app.settings.save();
  }
  dy += 36;
  // ---- hand summary ----
  {
    const RecPlayer* hero = nullptr;
    for (auto& p : rec->players)
      if (p.seat == rec->heroSeat) hero = &p;
    double net = static_cast<double>(rec->heroNet()) / rec->bb;
    int finalPot = rec->steps.back().potTotal();
    Rectangle box{di.x, dy, W, 96};
    DrawRectangleRounded(box, 0.12f, 8, theme::surface2);
    DrawRectangleRounded({box.x, box.y, 5, box.height}, 1, 4, net > 0 ? theme::gain : net < 0 ? theme::loss : theme::muted);
    g.text(net > 0 ? "You won" : net < 0 ? "You lost" : "Break-even", box.x + 20, box.y + 12, 13, theme::muted);
    g.text(fmtBB(net), box.x + 20, box.y + 32, 26, net > 0 ? theme::gain : net < 0 ? theme::loss : theme::text, true);
    if (hero) {
      g.text("Your hand", box.x + 200, box.y + 12, 13, theme::muted);
      g.card(hero->hole[0], {box.x + 200, box.y + 32, 34, 48});
      g.card(hero->hole[1], {box.x + 238, box.y + 32, 34, 48});
      g.text(hero->position, box.x + 282, box.y + 46, 15, theme::text2, true);
    }
    float rx = box.x + 360;
    g.text("Pot " + fmtInt(finalPot) + "  \xC2\xB7  " + (rec->uncontested ? std::string("won without a showdown") : "went to showdown"), rx, box.y + 12, 13, theme::muted);
    float wy = box.y + 34;
    for (auto& [seat, amt] : rec->winnings) {
      std::string nm = seat == rec->heroSeat ? "You" : "";
      for (auto& p : rec->players)
        if (p.seat == seat && seat != rec->heroSeat) nm = p.name;
      auto sdn = rec->showdownNames.find(seat);
      std::string line = nm + " won " + fmtInt(amt) + (sdn != rec->showdownNames.end() ? " with " + sdn->second : "");
      g.text(g.ellipsize(line, box.x + box.width - rx - 14, 14.5f, true), rx, wy, 14.5f, theme::text, true);
      wy += 22;
    }
    dy += box.height + 12;

    // ---- key moment: the costliest decision ----
    const RecDecision* key = nullptr;
    for (auto& d : rec->decisions)
      if (static_cast<int>(d.grade) >= 2 && (!key || d.evLossBB > key->evLossBB)) key = &d;
    if (key) {
      std::string when = key->street == Street::Preflop ? "Before the flop" : "On the " + std::string(streetName(key->street));
      std::string txt = when + " you chose " + key->heroLabel + "; the coach preferred " + key->bestLabel + ".";
      float th = g.wrappedHeight(txt, W - 190, 14) + 40;
      Rectangle kb{di.x, dy, W, th};
      DrawRectangleRounded(kb, 0.15f, 8, Color{58, 40, 30, 255});
      DrawRectangleRounded({kb.x, kb.y, 5, kb.height}, 1, 4, theme::serious);
      g.text("Key moment", kb.x + 20, kb.y + 10, 14, theme::serious, true);
      if (key->hasEvLoss && key->evLossBB > 0.05) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "-%.1f bb EV", key->evLossBB);
        g.text(buf, kb.x + 120, kb.y + 11, 13, theme::text2);
      }
      g.textWrapped(txt, kb.x + 20, kb.y + 30, W - 190, 14, theme::text);
      if (g.button({kb.x + kb.width - 150, kb.y + (kb.height - 32) / 2, 136, 32}, "Jump to it", Btn::Normal, true, 13)) step_ = std::max(0, key->step - 1);
      dy += th + 12;
    }

    // ---- street-by-street timeline ----
    g.text("Street by street", di.x, dy, 16, theme::text, true);
    dy += 28;
    static const char* names[] = {"Pre-flop", "Flop", "Turn", "River"};
    for (int st = 0; st < 4; ++st) {
      std::vector<std::pair<int, std::string>> acts;  // step index, label
      int potStart = -1;
      for (size_t i = 1; i < rec->steps.size(); ++i) {
        const Snapshot& sp = rec->steps[i];
        if (sp.result || sp.seat < 0) continue;
        int as = sp.actStreet >= 0 ? sp.actStreet : static_cast<int>(sp.street);
        if (as != st) continue;
        if (potStart < 0) potStart = rec->steps[i - 1].potTotal();
        acts.push_back({static_cast<int>(i), sp.label});
      }
      int nb = st == 0 ? 0 : st == 1 ? 3 : st == 2 ? 4 : 5;
      bool reached = st == 0 || static_cast<int>(rec->board.size()) >= nb;
      if (!reached) break;
      float cardsW = nb * 30.f;
      float lx = di.x + 100 + std::max(cardsW, 60.f) + 16;
      float aw = di.x + W - lx;
      // measure wrapped chips
      float cx = 0, rows = 1;
      for (auto& a : acts) {
        float w = g.measure(a.second, 12.5f).x + 16;
        if (cx > 0 && cx + w > aw) {
          cx = 0;
          rows++;
        }
        cx += w + 6;
      }
      float rh = std::max(52.f, rows * 30 + 14);
      Rectangle row{di.x, dy, W, rh};
      DrawRectangleRounded(row, 0.12f, 6, theme::surface2);
      g.text(names[st], row.x + 12, row.y + 10, 14, theme::accent, true);
      g.text(potStart >= 0 ? "pot " + fmtInt(potStart) : "", row.x + 12, row.y + 30, 12, theme::muted);
      for (int k = 0; k < nb; ++k) g.card(rec->board[k], {row.x + 100 + k * 30.f, row.y + 8, 27, 38});
      cx = 0;
      float cy = row.y + 10;
      if (acts.empty()) g.text(st == 0 ? "" : "no betting (all-in or checked through)", lx, cy + 4, 12.5f, theme::muted);
      for (auto& a : acts) {
        float w = g.measure(a.second, 12.5f).x + 16;
        if (cx > 0 && cx + w > aw) {
          cx = 0;
          cy += 30;
        }
        Rectangle chip{lx + cx, cy, w, 24};
        bool on = a.first == step_;
        bool hero = rec->steps[a.first].seat == rec->heroSeat;
        DrawRectangleRounded(chip, 0.4f, 6, on ? theme::accent : hero ? Color{44, 62, 52, 255} : theme::surface3);
        g.textCentered(a.second, chip, 12.5f, on ? theme::accentInk : theme::text2);
        if (g.clicked(chip)) step_ = a.first;
        cx += w + 6;
      }
      dy += rh + 8;
    }
    dy += 14;
    g.text("Replay", di.x, dy, 16, theme::text, true);
    dy += 28;
  }
  // board strip
  Rectangle strip{di.x, dy, W, 92};
  DrawRectangleRounded(strip, 0.15f, 8, theme::feltDark);
  for (int k = 0; k < 5; ++k) {
    Rectangle cr{strip.x + 14 + k * 54.f, strip.y + 10, 48, 68};
    if (k < static_cast<int>(sn.board.size())) g.card(sn.board[k], cr);
    else g.cardSlot(cr);
  }
  g.text("Pot " + fmtInt(sn.potTotal()), strip.x + 300, strip.y + 22, 16, theme::text, true);
  g.text(g.ellipsize(sn.label, W - 320, 14), strip.x + 300, strip.y + 50, 14, theme::text2);
  dy += 104;
  // controls
  float bx = di.x;
  if (g.button({bx, dy, 40, 32}, "|\xE2\x97\x80", Btn::Ghost, step_ > 0, 12)) step_ = 0;
  if (g.button({bx + 44, dy, 40, 32}, "\xE2\x97\x80", Btn::Ghost, step_ > 0, 12)) step_--;
  g.text("Step " + std::to_string(step_ + 1) + " / " + std::to_string(rec->steps.size()), bx + 96, dy + 8, 14, theme::text2);
  if (g.button({bx + 196, dy, 40, 32}, "\xE2\x96\xB6", Btn::Ghost, step_ + 1 < static_cast<int>(rec->steps.size()), 12)) step_++;
  if (g.button({bx + 240, dy, 40, 32}, "\xE2\x96\xB6|", Btn::Ghost, step_ + 1 < static_cast<int>(rec->steps.size()), 12)) step_ = static_cast<int>(rec->steps.size()) - 1;
  if (!touchUI()) g.text("Left/Right arrow keys", bx + 296, dy + 9, 12.5f, theme::muted);
  dy += 44;
  // players table
  float cols[] = {0, 200, 260, 360, 450, 520};
  const char* heads[] = {"Player", "Pos", "Cards", "Stack", "Bet", ""};
  for (int i = 0; i < 6; ++i) g.text(heads[i], di.x + cols[i], dy, 12.5f, theme::muted, true);
  dy += 22;
  bool isLast = step_ == static_cast<int>(rec->steps.size()) - 1;
  for (auto& p : rec->players) {
    auto it = sn.players.find(p.seat);
    if (it == sn.players.end()) continue;
    const SnapPlayer& sp = it->second;
    Rectangle row{di.x - 4, dy - 4, W + 4, 38};
    if (sn.seat == p.seat) DrawRectangleRounded(row, 0.2f, 6, withAlpha(theme::accent, 0.12f));
    Color ink = sp.folded ? theme::muted : theme::text;
    std::string nm = p.name;
    if (reveal && !p.profile.empty()) nm += "  (" + profile(p.profile).label + ")";
    g.text(g.ellipsize(nm, 190, 13.5f), di.x + cols[0], dy + 6, 13.5f, ink, p.seat == rec->heroSeat);
    g.text(p.position, di.x + cols[1], dy + 6, 12.5f, theme::text2, true);
    bool show = p.seat == rec->heroSeat || reveal || (isLast && rec->showdownNames.count(p.seat));
    g.card(p.hole[0], {di.x + cols[2], dy - 2, 24, 34}, !show, false, sp.folded);
    g.card(p.hole[1], {di.x + cols[2] + 27, dy - 2, 24, 34}, !show, false, sp.folded);
    g.text(fmtInt(sp.stack), di.x + cols[3], dy + 6, 13.5f, ink);
    if (sp.bet) g.text(fmtInt(sp.bet), di.x + cols[4], dy + 6, 13.5f, ink);
    std::string extra;
    if (isLast && rec->showdownNames.count(p.seat)) extra = rec->showdownNames.at(p.seat);
    else if (sp.folded) extra = "folded";
    if (isLast) {
      auto nt = rec->nets.find(p.seat);
      if (nt != rec->nets.end() && nt->second != 0) {
        std::string ns = (nt->second > 0 ? "+" : "") + fmtInt(nt->second);
        g.textRight(ns, di.x + W - 4, dy + 6, 13.5f, nt->second > 0 ? theme::gain : theme::loss, true);
      }
    }
    g.text(g.ellipsize(extra, W - cols[5] - 80, 13), di.x + cols[5], dy + 6, 13, theme::muted);
    dy += 40;
  }
  dy += 6;
  dy += 14;
  g.text("Coach review", di.x, dy, 17, theme::text, true);
  dy += 30;
  if (rec->decisions.empty()) {
    dy += g.textWrapped("No graded decisions in this hand (you may have folded preflop with nothing to decide).", di.x, dy, W, 14, theme::muted);
  }
  for (auto& d : rec->decisions) {
    float hh = 74;
    std::string who = "You: " + d.heroLabel + (d.grade != GradeLevel::Best ? "  \xC2\xB7  Coach: " + d.bestLabel : "");
    hh += g.wrappedHeight(who, W - 28, 13.5f, true);
    for (auto& l : d.lines) hh += g.wrappedHeight(l, W - 40, 13) + 4;
    Rectangle card{di.x, dy, W, hh};
    bool current = d.step == step_ + 1 || d.step == step_;
    DrawRectangleRounded(card, 0.06f, 8, theme::surface2);
    if (current) DrawRectangleRoundedLinesEx(card, 0.06f, 8, 1.5f, theme::accent);
    float cy = dy + 12;
    std::string st = streetName(d.street);
    if (g.button({card.x + 12, cy, g.measure(st, 13, true).x + 20, 26}, st, Btn::Ghost, true, 13)) step_ = std::max(0, d.step - 1);
    g.gradeBadge(card.x + 24 + g.measure(st, 13, true).x + 20, cy + 1, static_cast<int>(d.grade));
    if (d.hasEvLoss && d.grade != GradeLevel::Best && d.evLossBB > 0.05) {
      char buf[32];
      std::snprintf(buf, sizeof buf, "-%.1f bb EV", d.evLossBB);
      g.textRight(buf, card.x + card.width - 14, cy + 5, 13, theme::text2);
    }
    cy += 38;
    cy += g.textWrapped(who, card.x + 14, cy, W - 28, 13.5f, theme::text, true) + 4;
    for (auto& l : d.lines) {
      g.text("\xE2\x80\xA2", card.x + 14, cy, 13, theme::muted);
      cy += g.textWrapped(l, card.x + 26, cy, W - 40, 13, theme::text2) + 4;
    }
    dy += hh + 10;
  }
  detailScroll_.end(g, dy - dy0 + 10);
}

}  // namespace hl::ui
