// Learn tab: how to play, hand rankings, glossary and tips — written for complete beginners.
#include <algorithm>

#include "core/evaluator.hpp"
#include "ui/app.hpp"
#include "ui/course.hpp"

namespace hl::ui {

namespace {
struct Ranking {
  const char* name;
  const char* cards;
  const char* desc;
};
const Ranking RANKINGS[] = {
    {"Royal flush", "As Ks Qs Js Ts", "A, K, Q, J, 10 all of the same suit. The best possible hand."},
    {"Straight flush", "9h 8h 7h 6h 5h", "Five cards in a row, all of the same suit."},
    {"Four of a kind", "Qc Qd Qh Qs 7d", "Four cards of the same rank."},
    {"Full house", "Ks Kh Kd 4c 4s", "Three of a kind plus a pair."},
    {"Flush", "Ad Jd 8d 6d 2d", "Any five cards of the same suit."},
    {"Straight", "Tc 9d 8h 7s 6c", "Five cards in a row, any suits. A can be high (A-K-Q-J-10) or low (5-4-3-2-A)."},
    {"Three of a kind", "7c 7d 7h Ks 2d", "Three cards of the same rank."},
    {"Two pair", "Js Jh 5d 5c Ah", "Two different pairs."},
    {"One pair", "As Ad Qc 8h 3s", "Two cards of the same rank."},
    {"High card", "Ah Jc 9d 6s 3c", "None of the above: your highest card plays."},
};

struct Section {
  const char* title;
  const char* body;
};
const Section GLOSSARY[] = {
    {"Pot", "All the chips bet so far in the current hand. The winner takes it."},
    {"Blinds", "Forced bets posted before the cards are dealt: the small blind (SB) and big blind (BB)."},
    {"bb (big blind)", "The size of the big blind. Stacks and results are often measured in big blinds, e.g. '100bb'."},
    {"Button (BTN / D)", "The dealer position. It acts last after the flop - the best seat at the table."},
    {"Position", "Where you sit relative to the button. UTG/LJ act early, HJ/CO in the middle, BTN last; SB and BB are the blinds."},
    {"Pre-flop / Flop / Turn / River", "The four betting rounds: before any shared cards, after 3, after 4 and after all 5."},
    {"Kicker", "A side card that breaks ties. With A-K on an A-7-2 board you have a pair of aces with a king kicker."},
    {"Draw", "A hand that is not made yet but could improve, e.g. four cards to a flush."},
    {"Outs", "The unseen cards that would complete your draw. Rule of thumb: outs x 2 = % to hit on the next card, outs x 4 = % by the river from the flop."},
    {"Pot odds", "The price of calling. If you must call 10 to win 30, you need to win at least 10 / (30 + 10) = 25% of the time."},
    {"Equity / chance to win", "How often your hand would win if all the cards were dealt out, against the hands your opponents are likely to have."},
    {"Range", "All the hands a player could have, given what they have done so far. The bots and coach think in ranges."},
    {"Bluff", "Betting with a weak hand to make better hands fold."},
    {"Value bet", "Betting with a strong hand so that worse hands pay you."},
    {"C-bet", "Continuation bet: betting the flop after raising before the flop."},
    {"3-bet", "A re-raise of the first raise before the flop."},
    {"Limp", "Just calling the big blind before the flop instead of raising. Usually a weak play."},
    {"VPIP / PFR / AF", "HUD stats: how often a player puts money in pre-flop (VPIP), raises pre-flop (PFR), and how aggressive they are after the flop (AF)."},
    {"EV", "Expected value: how much an action wins or loses on average if you repeated it many times."},
    {"SPR", "Stack-to-pot ratio: your remaining chips divided by the pot. Low SPR means you are close to all-in."},
    {"Tilt", "Playing worse because of frustration after losing. The bots can tilt too!"},
};

const char* TIPS[] = {
    "Play fewer hands. Most starting hands lose money - folding is the most common correct move.",
    "When you play, prefer raising to just calling before the flop.",
    "Pairs, high cards (A, K, Q) and suited connected cards (like 9-8 of hearts) are the best starting hands.",
    "Play more hands on the button and fewer in early seats.",
    "Before calling a bet, compare your chance to win with the price (the side panel shows both).",
    "Bet your strong hands: checking them lets opponents see free cards.",
    "Don't bluff calling stations - they call almost everything.",
    "If a quiet, tight player suddenly raises big, believe them.",
    "Take your time: in beginner mode the coach suggests a move and explains why before you act.",
};

}  // namespace

float drawRankingsList(Gui& g, float x, float y, float width, float cardW) {
  float y0 = y;
  float cardH = cardW * 1.4f;
  for (size_t i = 0; i < std::size(RANKINGS); ++i) {
    const Ranking& rk = RANKINGS[i];
    g.text(std::to_string(i + 1) + ".", x, y + cardH / 2 - 10, 16, theme::muted, true);
    auto cs = parseCards(rk.cards);
    for (size_t k = 0; k < cs.size(); ++k) g.card(cs[k], {x + 34 + k * (cardW + 4), y, cardW, cardH});
    float tx = x + 34 + 5 * (cardW + 4) + 16;
    g.text(rk.name, tx, y + 2, 16, theme::text, true);
    float dh = g.textWrapped(rk.desc, tx, y + 24, width - (tx - x), 13, theme::text2);
    y += std::max(cardH, 24 + dh) + 12;
  }
  return y - y0;
}

float LearnScreen::drawBlock(App& app, const Block& b, float x, float y, float W) {
  Gui& g = app.g;
  float y0 = y;
  switch (b.type) {
    case Block::Heading:
      y += 6;
      g.text(b.a, x, y, 19, theme::accent, true);
      y += 32;
      break;
    case Block::Para:
      y += g.textWrapped(b.a, x, y, W, 15, theme::text2, false, 1.4f) + 12;
      break;
    case Block::Bullets: {
      size_t pos = 0;
      std::string text = b.b;
      while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        DrawCircleV({x + 6, y + 9}, 3, theme::accent);
        y += g.textWrapped(line, x + 20, y, W - 20, 15, theme::text2, false, 1.4f) + 8;
        if (nl == std::string::npos) break;
        pos = nl + 1;
      }
      y += 6;
      break;
    }
    case Block::Cards: {
      float cw = 46, ch = cw * 1.4f, gx = x;
      size_t pos = 0;
      std::string spec = b.a;
      float rowTop = y;
      while (pos <= spec.size()) {
        size_t bar = spec.find('|', pos);
        std::string grp = spec.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
        size_t colon = grp.find(':');
        std::string label = colon == std::string::npos ? "" : grp.substr(0, colon);
        std::string cards = colon == std::string::npos ? grp : grp.substr(colon + 1);
        std::vector<std::string> toks;
        std::string tok;
        for (char ch2 : cards) {
          if (ch2 == ' ') {
            if (!tok.empty()) toks.push_back(tok);
            tok.clear();
          } else
            tok += ch2;
        }
        if (!tok.empty()) toks.push_back(tok);
        float gw = std::max(g.measure(label, 12.5f, true).x, toks.size() * (cw + 4.f));
        if (gx > x && gx + gw > x + W) {
          gx = x;
          rowTop += ch + 34;
        }
        g.text(label, gx, rowTop, 12.5f, theme::muted, true);
        for (size_t k = 0; k < toks.size(); ++k) {
          Rectangle cr{gx + k * (cw + 4), rowTop + 20, cw, ch};
          if (toks[k] == "?") g.card(0, cr, true);
          else g.card(parseCard(toks[k]), cr);
        }
        gx += gw + 26;
        if (bar == std::string::npos) break;
        pos = bar + 1;
      }
      y = rowTop + 20 + ch + 18;
      break;
    }
    case Block::Tip:
    case Block::Example: {
      bool tip = b.type == Block::Tip;
      float bodyH = g.wrappedHeight(b.b, W - 40, 14.5f, false, 1.4f);
      float h = 46 + bodyH + 12;
      Rectangle r{x, y, W, h};
      DrawRectangleRounded(r, 0.08f, 8, tip ? Color{45, 40, 22, 255} : theme::surface2);
      if (tip) DrawRectangleRounded({x, y, 5, h}, 1, 4, theme::accent);
      else DrawRectangleRoundedLinesEx(r, 0.08f, 8, 1, theme::border);
      g.text(std::string(tip ? "\xE2\x98\x85 " : "") + b.a, x + 20, y + 14, 15.5f, tip ? theme::accent : theme::text, true);
      g.textWrapped(b.b, x + 20, y + 42, W - 40, 14.5f, theme::text2, false, 1.4f);
      y += h + 14;
      break;
    }
    case Block::Rows: {
      float lw = std::min(260.f, W * 0.34f);
      size_t pos = 0;
      std::string text = b.b;
      bool odd = false;
      while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        size_t bar = line.find('|');
        std::string left = line.substr(0, bar), right = bar == std::string::npos ? "" : line.substr(bar + 1);
        float rh = std::max(g.wrappedHeight(left, lw - 20, 14.5f, true), g.wrappedHeight(right, W - lw - 24, 14.5f, false, 1.35f)) + 16;
        if (odd) DrawRectangleRec({x, y, W, rh}, Color{255, 255, 255, 8});
        g.textWrapped(left, x + 12, y + 8, lw - 20, 14.5f, theme::text, true);
        g.textWrapped(right, x + lw, y + 8, W - lw - 24, 14.5f, theme::text2, false, 1.35f);
        DrawLineEx({x, y + rh}, {x + W, y + rh}, 1, theme::grid);
        y += rh;
        odd = !odd;
        if (nl == std::string::npos) break;
        pos = nl + 1;
      }
      y += 16;
      break;
    }
    case Block::Rankings:
      y += drawRankingsList(g, x, y, W, 42) + 6;
      break;
    case Block::Drill: {
      int k = std::atoi(b.a.c_str());
      float w = g.measure(b.b, 14, true).x + 60;
      if (g.button({x, y, w, 38}, "\xE2\x96\xB6  " + b.b, Btn::Primary, true, 14)) {
        app.tab = Tab::Drills;
        app.drills().open(k);
      }
      y += 52;
      break;
    }
  }
  return y - y0;
}

void LearnScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  const auto& chapters = course();
  const int nCh = static_cast<int>(chapters.size());
  const int total = nCh + 2;  // + glossary + tips
  section_ = std::clamp(section_, 0, total - 1);

  // ---- chapter list ----
  float navW = std::min(300.f, area.width * 0.27f);
  Rectangle nav{area.x, area.y, navW, area.height};
  g.panel(nav);
  int done = 0;
  for (auto& ch : chapters) done += app.progress.learnRead.count(ch.key) ? 1 : 0;
  g.text("Poker course", nav.x + 16, nav.y + 14, 17, theme::text, true);
  g.text(std::to_string(done) + " of " + std::to_string(nCh) + " chapters read", nav.x + 16, nav.y + 38, 12.5f, theme::muted);
  Rectangle bar{nav.x + 16, nav.y + 58, navW - 32, 6};
  DrawRectangleRounded(bar, 1, 6, theme::surface3);
  DrawRectangleRounded({bar.x, bar.y, bar.width * done / std::max(1, nCh), bar.height}, 1, 6, theme::accent);
  Rectangle list{nav.x + 6, nav.y + 76, navW - 12, nav.height - 84};
  float ly0 = navScroll_.begin(g, list);
  float ly = ly0;
  for (int i = 0; i < total; ++i) {
    std::string title = i < nCh ? chapters[i].title : i == nCh ? "Glossary" : "Beginner tips";
    bool read = i < nCh && app.progress.learnRead.count(chapters[i].key);
    Rectangle row{list.x, ly, list.width - 8, 36};
    bool on = i == section_;
    if (on || g.hover(row)) DrawRectangleRounded(row, 0.25f, 6, on ? theme::surface3 : theme::surface2);
    if (on) DrawRectangleRounded({row.x, row.y + 6, 3, row.height - 12}, 1, 4, theme::accent);
    if (i == nCh) DrawLineEx({row.x + 8, row.y - 3}, {row.x + row.width - 8, row.y - 3}, 1, theme::border);
    g.text(g.ellipsize(title, row.width - 46, 13.5f, on), row.x + 12, row.y + 10, 13.5f, on ? theme::text : theme::text2, on);
    if (read) g.textRight("\xE2\x9C\x93", row.x + row.width - 10, row.y + 9, 15, theme::gain, true);
    if (g.clicked(row)) {
      section_ = i;
      scroll_.toTop();
    }
    ly += 40;
  }
  navScroll_.end(g, ly - ly0);

  // ---- content ----
  Rectangle body{area.x + navW + 16, area.y, area.width - navW - 16, area.height};
  float W = std::min(860.f, body.width - 30);
  float x = body.x + std::max(0.f, (body.width - 14 - W) / 2);
  float y0 = scroll_.begin(g, body);
  float y = y0 + 4;
  if (section_ < nCh) {
    const Chapter& ch = chapters[section_];
    g.text(ch.title, x, y, 26, theme::text, true);
    y += 40;
    y += g.textWrapped(ch.summary, x, y, W, 15, theme::muted) + 18;
    DrawLineEx({x, y}, {x + W, y}, 1, theme::border);
    y += 18;
    for (const Block& b : ch.blocks) y += drawBlock(app, b, x, y, W);
    y += 16;
    if (section_ > 0 && g.button({x, y, 150, 40}, "\xE2\x97\x80  Previous", Btn::Ghost, true, 14)) {
      --section_;
      scroll_.toTop();
    }
    std::string nextLabel = section_ + 1 < nCh ? "Mark as read & next  \xE2\x96\xB6" : "Mark as read  \xE2\x9C\x93";
    float nw = g.measure(nextLabel, 14, true).x + 40;
    if (g.button({x + W - nw, y, nw, 40}, nextLabel, Btn::Primary, true, 14)) {
      app.progress.learnRead.insert(ch.key);
      app.progress.save();
      if (section_ + 1 < nCh) {
        ++section_;
        scroll_.toTop();
      } else
        app.showToast("Course complete - well done!");
    }
    y += 60;
  } else if (section_ == nCh) {
    g.text("Glossary", x, y, 26, theme::text, true);
    y += 44;
    for (auto& s2 : GLOSSARY) {
      g.text(s2.title, x, y, 15, theme::text, true);
      y += 22;
      y += g.textWrapped(s2.body, x + 16, y, W - 16, 14.5f, theme::text2, false, 1.35f) + 12;
    }
  } else {
    g.text("Beginner tips", x, y, 26, theme::text, true);
    y += 44;
    int n = 1;
    for (const char* t : TIPS) {
      g.text(std::to_string(n++) + ".", x, y, 16, theme::accent, true);
      y += g.textWrapped(t, x + 30, y, W - 30, 15, theme::text2, false, 1.4f) + 14;
    }
  }
  scroll_.end(g, y - y0 + 20);
}

}  // namespace hl::ui
