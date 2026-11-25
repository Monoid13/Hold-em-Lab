#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "ui/app.hpp"

namespace hl::ui {

void DrillsScreen::next() {
  q_ = generateDrill(static_cast<DrillKind>(active_), rng_);
  result_.reset();
  outsText_.clear();
  guess_ = 50;
}

void DrillsScreen::answer(App& app, int v) {
  if (result_ || !q_) return;
  answered_ = v;
  result_ = gradeDrill(*q_, v);
  app.progress.recordDrill(drillInfo(q_->kind).key, result_->correct, result_->score);
}

void DrillsScreen::handleKeys(App& app) {
  if (active_ < 0) return;
  if (result_ && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) next();
  else if (!result_ && q_ && q_->kind == DrillKind::Outs) {
    int ch;
    while ((ch = GetCharPressed()) > 0)
      if (ch >= '0' && ch <= '9' && outsText_.size() < 2) outsText_ += static_cast<char>(ch);
    if (IsKeyPressed(KEY_BACKSPACE) && !outsText_.empty()) outsText_.pop_back();
    if (IsKeyPressed(KEY_ENTER) && !outsText_.empty()) answer(app, std::atoi(outsText_.c_str()));
  } else if (!result_ && q_ && q_->kind == DrillKind::Equity && IsKeyPressed(KEY_ENTER))
    answer(app, guess_);
  if (IsKeyPressed(KEY_ESCAPE)) active_ = -1;
}

void DrillsScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  if (active_ < 0) {
    g.text("Short, focused exercises. Each one trains a skill you use every hand.", area.x, area.y + 4, 15, theme::text2);
    float cw = (area.width - 28) / 3, ch = 150;
    for (int k = 0; k < static_cast<int>(DrillKind::Count); ++k) {
      const DrillInfo& di = drillInfo(static_cast<DrillKind>(k));
      Rectangle r{area.x + (k % 3) * (cw + 14), area.y + 40 + (k / 3) * (ch + 14), cw, ch};
      bool hv = g.hover(r);
      g.panel(r);
      if (hv) DrawRectangleRoundedLinesEx(r, 0.12f, 8, 1.5f, theme::accent);
      g.text(di.title, r.x + 16, r.y + 16, 17, theme::text, true);
      g.textWrapped(di.desc, r.x + 16, r.y + 46, r.width - 32, 13.5f, theme::text2);
      auto it = app.progress.drills.find(di.key);
      std::string s = it != app.progress.drills.end() && it->second.attempts
                          ? std::to_string(it->second.attempts) + " tries \xC2\xB7 " + pct(static_cast<double>(it->second.correct) / it->second.attempts) + " correct \xC2\xB7 best streak " +
                                std::to_string(it->second.bestStreak)
                          : "Not tried yet";
      g.text(s, r.x + 16, r.y + r.height - 28, 12.5f, theme::muted);
      if (hv && g.released) {
        active_ = k;
        next();
      }
    }
    return;
  }
  const DrillInfo& di = drillInfo(static_cast<DrillKind>(active_));
  if (g.button({area.x, area.y, 120, 34}, "\xE2\x86\x90 All drills", Btn::Ghost, true, 13)) {
    active_ = -1;
    return;
  }
  g.text(di.title, area.x + 136, area.y + 5, 20, theme::text, true);
  auto it = app.progress.drills.find(di.key);
  if (it != app.progress.drills.end() && it->second.attempts)
    g.textRight("Streak " + std::to_string(it->second.streak) + "  \xC2\xB7  " + pct(static_cast<double>(it->second.correct) / it->second.attempts) + " of " + std::to_string(it->second.attempts),
                area.x + area.width, area.y + 9, 14, theme::text2);
  if (!q_) return;
  const DrillQuestion& q = *q_;
  float W = std::min(820.f, area.width);
  Rectangle body{area.x, area.y + 50, W, area.height - 50};
  float y0 = scroll_.begin(g, body);
  float y = y0 + 4;
  const float cw = 58, ch = cw * 1.4f;
  auto cards = [&](const std::vector<Card>& cs, float x, float yy) {
    for (size_t i = 0; i < cs.size(); ++i) g.card(cs[i], {x + i * (cw + 6), yy, cw, ch});
  };
  auto choiceBtn = [&](Rectangle r, const std::string& label, int value, bool isRight) {
    Btn style = Btn::Normal;
    if (result_) {
      if (value == answered_) style = result_->correct ? Btn::Call : Btn::Fold;
    }
    bool clicked = g.button(r, label, style, true, 16) && !result_;  // keep answered choices readable
    if (result_ && isRight) DrawRectangleRoundedLinesEx({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 0.4f, 8, 2, theme::good);
    if (clicked) answer(app, value);
  };
  float x = body.x;
  y += g.textWrapped(q.prompt, x, y, W - 20, 18, theme::text) + 10;
  switch (q.kind) {
    case DrillKind::Preflop: {
      cards(q.hero, x, y);
      y += ch + 8;
      g.text(q.hand, x, y, 14, theme::muted);
      y += 30;
      for (size_t i = 0; i < q.choices.size(); ++i) {
        int c = q.choices[i];
        choiceBtn({x + i * 150.f, y, 136, 48}, q.choiceLabels[c], c, result_ && c == result_->best && !result_->correct);
      }
      y += 64;
      break;
    }
    case DrillKind::Equity: {
      g.text("You", x, y, 13, theme::muted);
      g.text("Villain", x + 180, y, 13, theme::muted);
      y += 20;
      cards(q.hero, x, y);
      g.text("vs", x + 136, y + ch / 2 - 10, 16, theme::muted, true);
      cards(q.villain, x + 180, y);
      y += ch + 12;
      if (!q.board.empty()) {
        g.text("Board", x, y, 13, theme::muted);
        y += 20;
        cards(q.board, x, y);
        y += ch + 12;
      }
      int v = result_ ? answered_ : guess_;
      if (!result_) g.slider01({x, y + 6, W - 260, 30}, guess_);
      else {
        Rectangle bar{x, y + 18, W - 260, 6};
        DrawRectangleRounded(bar, 1, 6, theme::surface3);
        DrawCircleV({bar.x + bar.width * v / 100.f, bar.y + 3}, 8, theme::muted);
        DrawCircleV({bar.x + bar.width * static_cast<float>(q.answer), bar.y + 3}, 8, theme::good);
      }
      g.text(std::to_string(v) + "%", x + W - 240, y + 8, 22, theme::text, true);
      if (g.button({x + W - 150, y + 4, 130, 38}, "Submit [Enter]", Btn::Primary, !result_, 14)) answer(app, guess_);
      y += 56;
      break;
    }
    case DrillKind::PotOdds: {
      for (size_t i = 0; i < q.options.size(); ++i)
        choiceBtn({x + i * 130.f, y, 116, 48}, std::to_string(q.options[i]) + "%", q.options[i], result_ && q.options[i] == q.answerInt && !result_->correct);
      y += 64;
      break;
    }
    case DrillKind::Outs: {
      g.text("Your hand", x, y, 13, theme::muted);
      y += 20;
      cards(q.hero, x, y);
      y += ch + 12;
      g.text("Board", x, y, 13, theme::muted);
      y += 20;
      cards(q.board, x, y);
      y += ch + 14;
      int cur = outsText_.empty() ? 0 : std::atoi(outsText_.c_str());
      if (g.button({x, y, 44, 40}, "\xE2\x88\x92", Btn::Normal, !result_ && cur > 0, 18)) outsText_ = std::to_string(std::max(0, cur - 1));
      Rectangle box{x + 50, y, 80, 40};
      DrawRectangleRounded(box, 0.3f, 6, theme::surface2);
      DrawRectangleRoundedLinesEx(box, 0.3f, 6, 1, result_ ? theme::border : theme::accent);
      std::string shown = result_ ? std::to_string(answered_) : outsText_.empty() ? std::string("?") : outsText_;
      g.textCentered(shown, box, 18, theme::text, true);
      if (g.button({x + 136, y, 44, 40}, "+", Btn::Normal, !result_ && cur < 25, 18)) outsText_ = std::to_string(std::min(25, outsText_.empty() ? 1 : cur + 1));
      if (!touchUI()) g.text("or type a number", x + 192, y + 12, 13, theme::muted);
      if (g.button({x + (touchUI() ? 196 : 330), y, 130, 40}, "Submit [Enter]", Btn::Primary, !result_ && !outsText_.empty(), 14)) answer(app, std::atoi(outsText_.c_str()));
      y += 54;
      if (result_) {
        g.text("Outs:", x, y, 13, theme::muted);
        y += 20;
        for (size_t i = 0; i < q.outCards.size(); ++i) g.card(q.outCards[i], {x + i * 34.f, y, 30, 42});
        y += 54;
      }
      break;
    }
    default: {
      g.text("Board", x, y, 13, theme::muted);
      y += 20;
      cards(q.board, x, y);
      y += ch + 14;
      for (size_t i = 0; i < q.hands.size(); ++i) {
        g.text("Player " + std::to_string(i + 1), x + i * 150.f, y, 13, theme::muted);
        cards(q.hands[i], x + i * 150.f, y + 20);
      }
      y += ch + 34;
      for (size_t i = 0; i < q.hands.size(); ++i)
        choiceBtn({x + i * 150.f, y, 136, 46}, "Player " + std::to_string(i + 1), static_cast<int>(i), result_ && q.answerInt == static_cast<int>(i) && !result_->correct);
      choiceBtn({x + q.hands.size() * 150.f, y, 136, 46}, "Split pot", -1, result_ && q.answerInt == -1 && !result_->correct);
      y += 62;
    }
  }
  if (result_) {
    float hh = 90;
    for (auto& l : result_->explanation) hh += g.wrappedHeight(l, W - 60, 14) + 6;
    Rectangle rr{x, y, W - 20, hh};
    DrawRectangleRounded(rr, 0.08f, 8, theme::surface2);
    DrawRectangle(static_cast<int>(rr.x), static_cast<int>(rr.y + 6), 4, static_cast<int>(rr.height - 12), result_->correct ? theme::good : theme::critical);
    std::string head = result_->correct ? "\xE2\x9C\x93 Correct" : "\xE2\x9C\x95 Not quite";
    if (q.kind == DrillKind::Equity) head += "  \xC2\xB7  score " + std::to_string(result_->score);
    g.text(head, rr.x + 18, rr.y + 12, 17, theme::text, true);
    float yy = rr.y + 42;
    for (auto& l : result_->explanation) {
      g.text("\xE2\x80\xA2", rr.x + 18, yy, 14, theme::muted);
      yy += g.textWrapped(l, rr.x + 32, yy, W - 60, 14, theme::text2) + 6;
    }
    if (g.button({rr.x + 18, yy + 4, 140, 36}, "Next [Enter]", Btn::Primary, true, 14)) next();
    y += hh + 16;
  }
  scroll_.end(g, y - y0 + 20);
}

}  // namespace hl::ui
