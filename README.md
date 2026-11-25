# Hold'em Lab

A desktop no-limit Texas Hold'em trainer for Windows, macOS and Linux. You play against bots that
think, bluff, tilt and adapt to how you play, and a built-in coach grades every decision you make.

It is written in C++17 with [raylib](https://www.raylib.com) for graphics. The result is a single
self-contained executable; the fonts are embedded in it.

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

- **How they decide:** preflop they use position-based charts and range-vs-range thresholds, with
  push/fold for short stacks. After the flop they estimate each opponent's range from their actions
  and stats, then compare the expected value of every option using equity and fold equity.
- **No cheating:** bots only use public information: actions, position, bet sizes and HUD stats.
  They never see your cards.
- **They adapt to you:** your stats are tracked across sessions. If you fold too much, bots bluff
  you more; if you call too much, they value bet you thinner.
- **Human touches:** mixed strategies on close decisions, variable think times (snap folds, tanks,
  occasional "Hollywood" tanks with a monster), human bet sizes, tilt after bad beats, bet-size
  tells from weak players, and table chat.

## Download a ready-built app

Every push to GitHub builds the app for Windows, macOS and Linux:

1. Open the repository's **Actions** tab and choose the **Desktop app** workflow.
2. Open the latest successful run.
3. Under **Artifacts**, download `HoldemLab-windows`, `HoldemLab-macos` or `HoldemLab-linux`.
4. Unzip it and run `HoldemLab`.

- **macOS:** the app isn't signed, so the first time you open it, right-click it and choose
  **Open**, or run `xattr -d com.apple.quarantine HoldemLab` in Terminal.
- **Linux:** run `chmod +x HoldemLab` first.

## Build it yourself

You need CMake 3.16+, a C++17 compiler and git. Clone the repository, open a terminal in its folder,
and run the commands below. The first build downloads raylib automatically.

**Windows** (Visual Studio 2022 with "Desktop development with C++"), in a Developer PowerShell:

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

The app is `build\Release\HoldemLab.exe`.

**macOS** (needs the Xcode command line tools and `brew install cmake`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --parallel
```

```bash
./build/HoldemLab
```

**Linux** (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
```

```bash
./build/HoldemLab
```

If raylib 5 is already installed system-wide, CMake uses it instead of downloading it. Pass
`-DHOLDEM_BUILD_GUI=OFF` to build only the engine, tests and simulator (no graphics libraries
needed).

## Using the app

| Screen | What it does |
| --- | --- |
| **Play** | The table. Shortcuts: `F` fold, `C` check/call, `R` bet/raise, `Up`/`Down` change the size, `H` hint, `Space` next hand, `P` pause. The right-hand panel shows your hand, draws, outs, equity against estimated ranges, pot odds and the coach's grade for each decision. |
| **Review** | Replays recent hands step by step (`Left`/`Right` keys) with every hole card shown and the coach's notes on each of your decisions. |
| **Stats** | Profit graph, decision score by street, a leak finder, your HUD stats against typical winning ranges, results by position, drill scores, and reads on each opponent. |
| **Drills** | Preflop decisions, equity estimation, pot odds and bet math, counting outs, and reading showdowns. |
| **Ranges** | Opening charts for every position, plus a ranking of all 169 starting hands. |
| **Settings** | Number of opponents (1-8), table lineup or per-seat bot styles, cash game or tournament, stakes, bot speed, coach mode and training aids. |

Your settings, stats, hand histories and drill scores are saved automatically:

- **Windows:** `%APPDATA%\HoldemLab`
- **macOS:** `~/Library/Application Support/HoldemLab`
- **Linux:** `~/.local/share/holdemlab`

Set `HOLDEMLAB_DATA_DIR` to use a different folder.

## Tests and bot simulator

```bash
ctest --test-dir build -C Release --output-on-failure
```

```bash
./build/holdem_sim --hands 3000 --runs 4 --seats shark,tag,lag,station,nit,maniac
```

The simulator plays bots against each other and prints win rates and HUD stats per personality.
The table below is from 18,000 hands over six seeded runs in the 6-max mixed lineup:

| Bot | bb/100 |
| --- | ---: |
| Shark | +100 |
| LAG | +112 |
| TAG | +46 |
| Weak-Tight | -6 |
| Calling Station | -98 |
| Maniac | -155 |

Heads-up over 80,000 hands, the Shark beats the TAG by +14 bb/100 and the Calling Station by
+170 bb/100.

## Code layout

```
src/core/    cards, 7-card evaluator, ranges, equity, hand analysis, table engine
src/bots/    HUD stats, Bayesian range model, strategy (preflop + EV postflop), personalities, bot wrapper
src/coach/   spot analysis and decision grading
src/train/   drill generators and graders
src/app/     game controller, JSON persistence, settings, progress, hand history
src/ui/      raylib GUI: widgets and the six screens
tests/       dependency-free test runner
tools/       headless bot simulator
assets/      DejaVu fonts (embedded at build time; see assets/fonts/LICENSE)
```
