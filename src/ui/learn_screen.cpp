// Learn tab: how to play, hand rankings, glossary and tips — written for complete beginners.
#include <algorithm>

#include "core/evaluator.hpp"
#include "ui/app.hpp"

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
const Section HOW_TO_PLAY[] = {
    {"The goal", "Win chips. You win the pot (all the chips bet in a hand) by having the best five-card hand at the end, or by betting so that everyone else gives up (folds)."},
    {"Your cards", "Each player gets 2 private cards that only they can see. Up to 5 shared cards are then dealt face up in the middle. Your hand is the best 5 cards you can make from your 2 cards plus the 5 shared cards - you can use both, one or none of your own."},
    {"Blinds and the button", "Before the cards are dealt, the two players to the left of the dealer button (the white D) put in forced bets: the small blind and the big blind. This gives everyone something to play for. The button moves one seat to the left every hand, so everyone takes turns paying the blinds."},
    {"Four betting rounds", "Pre-flop: after the private cards. Flop: 3 shared cards are dealt. Turn: a 4th shared card. River: the 5th and final shared card. In each round players act one at a time, going around the table to the left."},
    {"Your options", "Fold: give up the hand. Check: pass without betting (only if nobody has bet). Call: put in the same amount as the current bet. Bet: put chips in first. Raise: bet more than the current bet. All-in: bet every chip you have."},
    {"Showdown", "If two or more players are still in after the river, they show their cards. The best five-card hand wins the pot; equal hands split it. If everyone else folds earlier, the last player left wins without showing."},
    {"Position matters", "Players who act later in a round see what everyone else did first. That is an advantage, so you can play more hands from late seats (like the button) and should be pickier in early seats."},
    {"How this app helps", "In beginner mode the side panel tells you what you have, how strong it is, and suggests a move with reasons. After every decision the coach says how good it was. The Review tab replays hands, Drills train single skills, and Ranges shows which starting hands to play from each seat."},
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

const char* SECTION_NAMES[] = {"How to play", "Hand rankings", "Words to know", "Beginner tips"};
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

void LearnScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  float x = area.x;
  for (int i = 0; i < 4; ++i) {
    float w = g.measure(SECTION_NAMES[i], 13.5f, true).x + 26;
    if (g.button({x, area.y, w, 32}, SECTION_NAMES[i], section_ == i ? Btn::ChipOn : Btn::Chip, true, 13.5f)) {
      section_ = i;
      scroll_.toTop();
    }
    x += w + 8;
  }
  Rectangle body{area.x, area.y + 46, area.width, area.height - 46};
  float W = std::min(900.f, body.width - 14);
  float y0 = scroll_.begin(g, body);
  float y = y0;
  if (section_ == 0) {
    for (auto& s : HOW_TO_PLAY) {
      g.text(s.title, body.x, y, 18, theme::accent, true);
      y += 28;
      y += g.textWrapped(s.body, body.x, y, W, 15, theme::text2) + 18;
    }
    if (g.button({body.x, y, 220, 38}, "See the hand rankings", Btn::Primary, true, 14)) {
      section_ = 1;
      scroll_.toTop();
    }
    y += 50;
  } else if (section_ == 1) {
    y += g.textWrapped("From best to worst. Your hand is always the best 5 cards from your 2 private cards plus the 5 shared cards. Suits never rank above each other.", body.x, y, W, 14.5f, theme::text2) + 16;
    y += drawRankingsList(g, body.x, y, W, 44) + 10;
  } else if (section_ == 2) {
    for (auto& s : GLOSSARY) {
      g.text(s.title, body.x, y, 15, theme::text, true);
      y += 22;
      y += g.textWrapped(s.body, body.x + 16, y, W - 16, 14, theme::text2) + 12;
    }
  } else {
    int n = 1;
    for (const char* t : TIPS) {
      std::string num = std::to_string(n++) + ".";
      g.text(num, body.x, y, 16, theme::accent, true);
      y += g.textWrapped(t, body.x + 30, y, W - 30, 15, theme::text2) + 14;
    }
  }
  scroll_.end(g, y - y0 + 20);
}

}  // namespace hl::ui
