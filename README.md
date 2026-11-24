# Hold'em Lab

A no-limit Texas Hold'em trainer that runs in your browser. You play against opponents that think,
bluff, tilt and adapt the way people do, and a built-in coach grades every decision you make.

No dependencies, no build step, no account. Everything is stored locally in your browser.

## Quick start

```bash
npm start
```

Then open <http://localhost:8080>. Node 18+ is required for the dev server and tests. Any static file
server works too (the app is plain HTML/CSS/ES modules); browsers block ES modules from `file://`,
so double-clicking `index.html` will not work.

## The opponents

Every bot runs on the same decision engine, and personalities sit on top of it as biases, so weaker
players make the mistakes real people make rather than random ones.

| Style | How it plays | How to beat it |
| --- | --- | --- |
| **Shark** | Close to balanced, mixes bluffs and value, adapts fastest to you | Play solid; avoid predictable lines |
| **TAG** | Tight-aggressive regular, slightly too tight | Steal more; attack its c-bets when it gives up |
| **LAG** | Loose-aggressive, lots of 3-bets and pressure | Widen your calling and 4-bet ranges, trap |
| **Nit** | Waits for premiums | Steal blinds, fold to its big bets |
| **Calling Station** | Calls far too much; bet size leaks hand strength | Value bet thinly, never bluff |
| **Maniac** | Raises and bluffs constantly | Call down lighter, let it bluff |
| **Weak-Tight** | Plays plenty of hands but folds to pressure | Bluff scary turns and rivers |

**How they decide**

- **Preflop:** position-based opening charts, range-vs-range thresholds for 3-bets, 4-bets and
  calls, push/fold for short stacks, and an EV calculation for any spot that commits the stack.
- **Postflop:** each opponent's hand range is estimated with Bayesian updates from their actions
  and observed stats. The bot then computes Monte Carlo equity against those ranges, estimates
  fold equity, and compares the EV of fold, check, call and every bet or raise size.
- **No cheating:** bots only use public information: actions, position, bet sizes and HUD stats.
  They never see your cards.
- **They adapt to you:** your VPIP, fold-to-bet and other stats are tracked across sessions. If you
  fold too much, bots bluff you more. If you call too much, they value bet you thinner.

**What makes them feel human**

- **Mixed strategies:** close decisions are randomised in proportion to their EV, so the bots are
  hard to read.
- **Variable think time:** snap folds with trash, tanks on hard river decisions, and occasional
  "Hollywood" tanks holding a monster.
- **Human bet sizes:** amounts are rounded to numbers people actually bet.
- **Tilt:** after a bad beat or a big loss, bots play looser and more aggressively for a while.
- **Tells and chat:** weak players size their bets by hand strength, and bots chat at the table.

You can check bot strength with the headless simulator:

```bash
npm run sim -- --hands 3000 --players 6 --runs 4 --seats shark,tag,lag,station,nit,maniac
```

## Training features

- **Coach:** grades each decision as Best, Good, Inaccuracy, Mistake or Blunder by its estimated
  EV loss, and explains why: equity against the estimated range, pot odds, outs, fold equity,
  break-even bluff frequency, and the strategy it would use. Ask for a hint (`H`) before you act.
- **Training HUD:** shows your made hand and draws, outs with rule-of-2-and-4 odds, equity against
  opponent ranges next to the equity you need to call, SPR, position and estimated range widths.
- **Opponent HUD:** shows VPIP / PFR / AF on every seat. The Stats page auto-reads each opponent
  and suggests an exploit; you can then reveal their real style to check your read.
- **Hand review:** step through the last 150 hands action by action, see every hole card, and
  read the coach's annotation on each of your decisions. Filter by mistakes, big pots or showdowns.
- **Stats and leak finder:** profit graph, win rate, decision score by street, your HUD stats
  against typical winning ranges, results by position, and a ranked list of your biggest leaks.
- **Drills:**
  - preflop open / 3-bet decisions
  - equity estimation
  - pot odds, MDF and break-even bluff math
  - counting outs
  - reading showdowns
- **Ranges:** opening charts for every position (full ring, 6-max and heads-up), plus a 169-hand
  strength ranking.
- **Game modes:** cash games with 2-9 players and preset or custom lineups, and Sit & Go
  tournaments with rising blinds and antes.

Keyboard shortcuts: `F` fold · `C` check/call · `R` bet/raise · `H` hint · `Space` next hand ·
`←`/`→` step through a replay.

## Project layout

```
index.html, css/style.css   UI shell and styles
js/core/                    cards, 7-card evaluator, equity, ranges, hand analysis, table engine
js/bots/                    stats tracking, range estimation, strategy, personalities, bot wrapper
js/coach/                   spot analysis and decision grading
js/train/                   drill generators and graders
js/app/                     game controller, settings, persistence, history, progress
js/ui/                      views (table, action bar, sidebar, review, stats, drills, ranges, settings)
scripts/                    dev server and bot simulator
tests/                      node:test suites
```

The table engine (`js/core/table.js`) implements full no-limit rules: heads-up blind order,
min-raises, incomplete all-in raises that don't reopen the action, uncalled-bet returns, side
pots and odd-chip splitting.

## Tests

```bash
npm test
```

The tests cover:

- an exhaustive check of all 2,598,960 five-card hands against the known category counts
- betting rules and side pots
- chip conservation over thousands of random hands
- range parsing and the range model
- bot legality across every personality
- coach grading
- drill generation

## License

MIT
