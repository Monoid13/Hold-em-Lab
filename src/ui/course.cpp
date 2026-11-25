#include "ui/course.hpp"

namespace hl::ui {

namespace {
Block H(const std::string& t) { return {Block::Heading, t, ""}; }
Block P(const std::string& t) { return {Block::Para, t, ""}; }
Block L(const std::string& lines) { return {Block::Bullets, "", lines}; }
Block C(const std::string& spec) { return {Block::Cards, spec, ""}; }
Block T(const std::string& title, const std::string& body) { return {Block::Tip, title, body}; }
Block E(const std::string& title, const std::string& body) { return {Block::Example, title, body}; }
Block R(const std::string& rows) { return {Block::Rows, "", rows}; }
Block RK() { return {Block::Rankings, "", ""}; }
Block D(int drill, const std::string& label) { return {Block::Drill, std::to_string(drill), label}; }

std::vector<Chapter> build() {
  std::vector<Chapter> c;

  c.push_back({"welcome", "1. Welcome to poker", "What poker is, why Texas Hold'em, and how to use this course.",
               {
                   P("Poker is a family of card games where players bet on who has the best hand. Unlike most casino games you play against other players, not the house, so good decisions win money over time - even though luck decides any single hand."),
                   P("Texas Hold'em is the most popular form of poker in the world. It is the game you see on TV and the one this app teaches. We play No-Limit Hold'em: on your turn you may bet any amount, up to every chip in front of you."),
                   H("The goal"),
                   P("Win chips. Every hand there is a pot - all the chips bet so far. You win it in one of two ways:"),
                   L("Have the best five-card hand when the cards are shown at the end (the showdown).\nMake a bet that nobody else is willing to match, so everyone else gives up (folds)."),
                   P("That second way is what makes poker special: you do not always need the best cards. You need to make better decisions than your opponents."),
                   H("Luck and skill"),
                   P("In one hand anything can happen - a weak hand can beat a strong one. Over hundreds of hands, though, the luck evens out and the player who folds bad hands, bets good ones and reads situations well comes out ahead. That is why this app grades your decisions, not your results."),
                   H("Cash games and tournaments"),
                   L("Cash game: chips are worth money, you can join or leave at any time, and the blinds (forced bets) never change.\nTournament: everyone starts with the same chips, the blinds rise over time, and you play until one player has all the chips."),
                   P("Hold'em Lab lets you play both - choose in Settings."),
                   T("How to use this course", "Read a chapter, then play a few hands to try it out. Chapters end with a link to the matching drill. A tick appears next to each chapter you finish."),
               }});

  c.push_back({"rankings", "2. Cards and hand rankings", "The deck, the ten hand types, kickers and split pots.",
               {
                   P("Poker uses a standard 52-card deck: 13 ranks in 4 suits. From lowest to highest the ranks are 2, 3, 4, 5, 6, 7, 8, 9, 10 (T), Jack, Queen, King, Ace. The Ace is the highest card, but it can also be the lowest card in the straight A-2-3-4-5. The suits (spades, hearts, diamonds, clubs) are all equal - no suit beats another."),
                   P("Your hand is always the best five cards you can make from your two private cards plus the five shared cards on the table. You can use both of your cards, one of them, or even none."),
                   H("The ten hands, best to worst"),
                   RK(),
                   H("Kickers: breaking ties"),
                   P("When two players have the same type of hand, the higher cards decide. With one pair, the higher pair wins; if the pairs are equal, the highest side card (the kicker) wins, then the next, and so on - always using only five cards."),
                   C("Board:Ah 9c 7d 4s 2h|Player 1:As Kd|Player 2:Ac Qh"),
                   P("Both players have a pair of aces. Player 1's king kicker beats Player 2's queen, so Player 1 wins."),
                   H("Split pots"),
                   P("If the best five cards are exactly equal, the pot is split. A common case is when the board itself is the best hand:"),
                   C("Board:Ts Js Qd Kc Ah|Player 1:2c 3d|Player 2:7h 7s"),
                   P("The board makes an ace-high straight. Nobody can beat it with their own cards, so everyone still in the hand splits the pot - even the pair of sevens plays the board."),
                   T("Common mix-ups", "A flush beats a straight. A full house beats a flush. Two pair is only ever two pairs plus one kicker - if three pairs are possible, the lowest pair does not count. And suits never break ties."),
                   D(4, "Practise: Read the showdown drill"),
               }});

  c.push_back({"hand", "3. How a hand is played", "Button, blinds, the four betting rounds and the showdown - with a full example.",
               {
                   H("The button and the blinds"),
                   P("One seat has the dealer button (the white D). It moves one seat to the left after every hand. The two players to its left post forced bets before any cards are dealt: the small blind (SB, usually half) and the big blind (BB). Without blinds nobody would have a reason to bet."),
                   H("The four betting rounds"),
                   R("Pre-flop|Everyone gets 2 private cards. Betting starts with the player left of the big blind.\nFlop|Three shared cards are dealt face up. Betting starts with the first active player left of the button.\nTurn|A fourth shared card. Another betting round.\nRiver|The fifth and last shared card. The final betting round.\nShowdown|Players still in show their cards; the best hand wins the pot."),
                   P("In each round the action moves clockwise. You can only check if nobody has bet yet in that round; once someone bets, everyone else must call, raise or fold. A round ends when everyone still in has put in the same amount."),
                   H("A complete example hand"),
                   E("Pre-flop", "Six players, blinds 1/2. You are on the button with A-K of spades. Two players fold, the cutoff raises to 6, you re-raise (3-bet) to 18, the blinds fold and the cutoff calls. Pot: 1 + 2 + 18 + 18 = 39."),
                   C("You:As Ks|Cutoff:? ?"),
                   E("Flop", "The flop comes K-7-2. You hit top pair with the best kicker. The cutoff checks, you bet 20 (about half the pot), the cutoff calls. Pot: 79."),
                   C("Board:Kh 7d 2c"),
                   E("Turn", "The 5 of spades. Both players check - you are happy to keep the pot moderate."),
                   C("Board:Kh 7d 2c 5s"),
                   E("River", "The queen of hearts. The cutoff bets 40. You call, because top pair with an ace kicker beats most hands that bet here."),
                   C("Board:Kh 7d 2c 5s Qh"),
                   E("Showdown", "The cutoff shows K-J for the same pair of kings, but your ace kicker plays: you win the 159-chip pot."),
                   C("You:As Ks|Cutoff:Kc Jd|Board:Kh 7d 2c 5s Qh"),
                   T("Watch it happen", "In the Play tab, beginner mode describes each round as it happens, and the Review tab replays any hand step by step."),
               }});

  c.push_back({"betting", "4. Betting rules", "Check, call, bet, raise, minimum raises, all-in and side pots.",
               {
                   R("Fold|Give up the hand. You lose what you already put in, nothing more.\nCheck|Pass without betting. Only allowed when nobody has bet in this round.\nCall|Match the current bet.\nBet|Put chips in when nobody else has this round.\nRaise|Increase the current bet. Everyone else must call the new amount, raise again, or fold.\nAll-in|Bet every chip you have."),
                   H("How much can you bet?"),
                   L("The smallest bet is one big blind.\nA raise must be at least as big as the previous bet or raise. If someone bets 10, the minimum raise is to 20; if they then raise to 30 (a raise of 20), the next raise must be to at least 50.\nThe largest bet is all your chips - that is the 'No-Limit' in No-Limit Hold'em."),
                   H("All-in and side pots"),
                   P("You can never be forced out of a hand because you do not have enough chips. If you call with less than the full bet, you are all-in and can only win the part of the pot you matched. The extra chips go into a side pot that only the other players can win."),
                   E("Side pot example", "Anna has 50 chips, Ben 100, Cleo 100. Anna goes all-in for 50, Ben and Cleo both put in 100. The main pot is 150 (50 from each) and all three can win it. The side pot is 100 (the extra 50 from Ben and Cleo) and only Ben or Cleo can win it. If Anna has the best hand she wins 150; the best of Ben and Cleo wins the other 100."),
                   P("If a bet is not called by anyone, the uncalled part is simply returned to the bettor."),
                   T("Good habit", "Decide your action before it is your turn, and act in one motion. In this app, the slider and the preset buttons (1/3, 1/2, 2/3, pot) set the size; the Bet/Raise button confirms it."),
               }});

  c.push_back({"position", "5. Position", "Why acting last is a big advantage, and what the seat names mean.",
               {
                   P("After the flop, the player closest to the left of the button acts first and the button acts last. Acting last is called being in position. It is one of the biggest advantages in poker: you see what everyone else does before you decide."),
                   R("UTG / LJ|Early position: act first before the flop. Many players still act behind you, so play only strong hands.\nHJ / CO|Middle and late position: you can open more hands.\nBTN|The button: acts last on every street after the flop. The best seat - play the most hands here.\nSB / BB|The blinds: already have chips in, but act first after the flop, which makes them hard seats to play."),
                   E("Why it matters", "With a medium hand out of position you often have to guess. In position you can check behind to see a free card, bet when your opponent shows weakness, or fold cheaply when they show strength."),
                   T("Rule of thumb", "The closer you sit to the button, the more hands you can play. The Ranges tab shows exactly which hands to open from each seat."),
               }});

  c.push_back({"starting", "6. Starting hands", "Which two-card hands to play, and how to enter the pot.",
               {
                   P("There are 169 different starting hands (counting suited and offsuit separately). Most of them lose money - the strongest players fold most hands before the flop."),
                   R("Premium pairs|AA, KK, QQ, JJ - raise and re-raise.\nBig aces|AK, AQ - very strong, especially suited.\nMedium pairs|TT down to 77 - strong; play them confidently.\nSuited broadways|KQs, KJs, QJs, JTs - play well after the flop.\nSmall pairs|66 down to 22 - hope to make three of a kind (a set); cheap to play in position.\nSuited connectors|98s, 87s, 76s - make straights and flushes; play them late and cheaply.\nTrash|Offsuit, unconnected low cards like 92o or J4o - fold."),
                   C("Premium:As Ah|Big ace:Ad Kd|Suited connector:8h 7h|Fold this:9c 2d"),
                   H("How to enter the pot"),
                   L("If nobody has raised yet, raise (about 2.5 big blinds) or fold. Just calling the big blind (limping) is usually a mistake.\nIf someone has raised, play tighter: re-raise your best hands, call with good hands that play well, fold the rest.\nThe big blind gets a discount - you already have one big blind in - so you can call raises with more hands there."),
                   T("Suited vs offsuit", "Suited hands (both cards the same suit, like A-5 of hearts) are worth a little more because they can make flushes, but not as much as many beginners think: about 2-3% more equity."),
                   D(0, "Practise: Preflop decisions drill"),
               }});

  c.push_back({"odds", "7. Pot odds, outs and equity", "The maths behind every call, made simple.",
               {
                   H("Equity"),
                   P("Your equity is how often your hand would win if all the cards were dealt out. A pair of aces has about 85% equity against one random hand; A-K against a pair of queens has about 46%."),
                   H("Outs"),
                   P("When you are behind, outs are the cards still in the deck that would give you the best hand."),
                   R("Flush draw (four of a suit)|9 outs\nOpen-ended straight draw (e.g. 9-8 on T-J-2)|8 outs\nGutshot (one card fills the middle)|4 outs\nTwo overcards to make a pair|about 6 outs\nFlush draw + open-ended straight draw|15 outs"),
                   C("You:9h 8h|Board:Th Jh 2c"),
                   P("Here you have a flush draw (9 hearts left) and an open-ended straight draw (any 7 or Q). Not double-counting the 7 and Q of hearts, that is 15 outs - you are actually a favourite against a single pair."),
                   H("The rule of 2 and 4"),
                   P("Multiply your outs by 4 on the flop (two cards to come, if you will see both) or by 2 on the turn (one card to come) to estimate your chance of hitting. 9 outs on the flop: about 36%. 9 outs on the turn: about 18%."),
                   H("Pot odds"),
                   P("Pot odds compare the price of a call with what you can win. Needed equity = amount to call / (pot after you call)."),
                   E("Worked example", "The pot is 60 and your opponent bets 20. The pot is now 80 and you must call 20 to win it, making the final pot 100. You need 20 / 100 = 20% equity. With a flush draw on the turn (about 18-20%) the call is close; on the flop with two cards to come it is clearly profitable."),
                   H("Implied odds"),
                   P("Sometimes a call is worth it even without the direct odds, because you will win extra chips later when you hit. Implied odds are biggest when your opponent has a big stack and a strong hand they will not fold."),
                   T("Let the app help", "During your turn the side panel shows your chance to win and the equity you need. The coach's explanations use exactly these numbers."),
                   D(2, "Practise: Pot odds & bet math drill"),
                   D(3, "Practise: Counting outs drill"),
               }});

  c.push_back({"board", "8. Reading the board", "Nuts, board textures and how strong your hand really is.",
               {
                   P("Your hand is only as strong as it is compared to what your opponents might hold. The same pair of kings can be a monster on one board and almost worthless on another."),
                   H("The nuts"),
                   P("The nuts is the best possible hand on the current board. On this board the nuts is A-K, which makes an ace-high straight. A set of queens feels strong, but it is behind A-K, K-9 and 9-8, which all make straights:"),
                   C("Board:Qs Js Td 4c"),
                   H("Board textures"),
                   R("Dry (e.g. K-7-2 rainbow)|Few draws possible. The first player to hit a pair is usually ahead.\nWet (e.g. 9-8-7 with two hearts)|Many straight and flush draws. Strong hands want to bet to charge draws.\nPaired (e.g. 8-8-3)|Trips and full houses become possible; hands are harder to read.\nMonotone (three cards of one suit)|A flush may already be made. Be careful with one-pair hands."),
                   C("Dry:Kc 7d 2h|Wet:9h 8h 7c|Paired:8s 8d 3c|Monotone:Ks 9s 4s"),
                   H("Relative strength"),
                   L("Top pair with a good kicker is strong on a dry board but only medium on a wet one.\nA set (pocket pair matching a board card) is very strong almost everywhere.\nWhen the board makes four to a straight or flush, one pair goes down in value a lot.\nIf a pair or two pair is on the board itself, everybody shares it - your own cards matter."),
                   T("In this app", "The side panel names your hand and adds the details: 'top pair, good kicker', 'set', 'pair on board' and the board texture."),
               }});

  c.push_back({"betting2", "9. Betting with a purpose", "Value bets, bluffs, continuation bets and bet sizing.",
               {
                   P("Every bet should have a reason. There are two good ones:"),
                   L("Value: you bet because worse hands will call. You want to get paid.\nBluff: you bet because better hands will fold. You want them to give up."),
                   P("If you bet and only better hands call while only worse hands fold, the bet has no purpose - check instead."),
                   H("Continuation bets"),
                   P("If you raised before the flop, a bet on the flop (a c-bet) often wins the pot straight away because your opponent usually missed. C-bet more on dry boards with high cards, less on wet, connected boards that help the caller."),
                   H("Semi-bluffs"),
                   P("Betting with a draw is a semi-bluff: you can win immediately if they fold, and you can still make the best hand if they call. Draws are the best bluffs."),
                   H("Choosing a size"),
                   R("1/3 pot|Small c-bets on dry boards; thin value; cheap bluffs.\n1/2 to 2/3 pot|The standard size for most value bets and semi-bluffs.\nPot|Strong hands on wet boards, to charge draws.\nOverbet (more than pot)|Very strong hands or bluffs on the river when the opponent's range is weak."),
                   H("The bluff math"),
                   P("A bluff of B into a pot of P only needs to work B / (P + B) of the time. A half-pot bluff needs 33% folds; a pot-sized bluff needs 50%. Against players who rarely fold, bluff less and value bet more."),
                   T("Don't bluff calling stations", "If a player calls everything, bluffs lose money. Bet your good hands bigger against them instead."),
               }});

  c.push_back({"opponents", "10. Playing your opponents", "HUD stats, player types and how to exploit them.",
               {
                   P("Every player has tendencies. Noticing them is how good players win more. Hold'em Lab tracks three key stats for every opponent (shown under their seat when 'Show opponent HUD stats' is on):"),
                   R("VPIP|How often they put money in before the flop. Around 20-30% is normal; 40%+ is very loose.\nPFR|How often they raise before the flop. A big gap between VPIP and PFR means they call too much.\nAF|Aggression after the flop: bets and raises divided by calls. Below 1 is passive, above 3 is aggressive."),
                   H("Common player types"),
                   R("Calling station|Calls almost everything. Value bet thinly and bigger; never bluff.\nNit (rock)|Plays very few hands. Steal their blinds; believe their big bets.\nManiac|Bets and raises constantly. Call down lighter and let them bluff.\nTAG (tight-aggressive)|Solid. Respect their raises; attack when they check.\nWeak-tight|Folds too often to pressure. Bluff them on scary cards."),
                   H("Bet-size tells"),
                   P("Weaker players often bet big with strong hands and small with weak ones. Several bots in this app do exactly that - watch for it."),
                   T("Test your reads", "The Stats tab gives an automatic read on every opponent. Turn on 'Reveal bot styles' in Settings to check whether you were right."),
               }});

  c.push_back({"habits", "11. Bankroll, tilt and good habits", "Staying in control, and how to keep improving.",
               {
                   H("Bankroll management"),
                   P("Only play with money you can afford to lose, and only at stakes your bankroll can survive. Swings are normal even for winning players. Common guidelines are 20-30 buy-ins for cash games and 100 or more buy-ins for tournaments."),
                   H("Tilt"),
                   P("Tilt is playing worse because of frustration - after a bad beat, a long losing run or an annoying opponent. Signs: playing hands you would normally fold, calling to 'catch' someone bluffing, chasing losses. The fix is simple: take a break. (The bots in this app tilt too: watch for one that suddenly plays wild after losing a big pot.)"),
                   H("How to improve with this app"),
                   L("Play with beginner mode on and read the coach's suggestion before you act.\nAfter a session, open Review, filter 'With mistakes' and replay those hands.\nCheck the Leak finder in Stats - it lists the mistakes you make most.\nSpend five minutes a day on Drills.\nWhen you feel ready, turn beginner mode off and move to the Tough table."),
                   T("Judge decisions, not results", "You can play perfectly and lose a hand, or play badly and win. Over time only the decisions matter - that is what the coach grades."),
               }});

  c.push_back({"tournaments", "12. Tournament basics", "Rising blinds, stack sizes and going all-in.",
               {
                   P("In a tournament (Sit & Go in this app) everyone starts with the same chips and the blinds increase every few hands. When you run out of chips you are out. The last player standing wins."),
                   H("Think in big blinds"),
                   P("What matters is how many big blinds you have, not how many chips. 2,000 chips is a lot at blinds 10/20 (100 big blinds) but very little at 200/400 (5 big blinds)."),
                   R("40bb+|Play normally.\n20-40bb|Tighten up a little; avoid marginal calls of big bets.\n10-20bb|Re-shove (go all-in over a raise) with good hands rather than calling.\nUnder 10-15bb|Push or fold: when you play a hand, go all-in. It makes the most of your fold equity."),
                   T("Survival matters", "Late in a tournament, finishing higher is worth more, so avoid risky calls for your whole stack unless you are clearly ahead."),
               }});

  return c;
}
}  // namespace

const std::vector<Chapter>& course() {
  static const std::vector<Chapter> c = build();
  return c;
}

}  // namespace hl::ui
