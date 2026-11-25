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

## Download

Get the latest version from the **[Releases page](https://github.com/Monoid13/Hold-em-Lab/releases/latest)**:

| Your computer | File |
| --- | --- |
| Windows 10 / 11 | `HoldemLab-windows.zip` |
| Mac (Apple Silicon or Intel, macOS 11+) | `HoldemLab-macos.zip` |
| Linux (x86-64) | `HoldemLab-linux.tar.gz` |

- **Windows:** unzip and double-click `HoldemLab.exe`. If SmartScreen says it "protected your
  PC", click **More info**, then **Run anyway**. The app isn't code-signed, which is why Windows
  asks.
- **Mac:** unzip and move `HoldemLab` to Applications. The first time, right-click it and choose
  **Open**. On recent macOS versions you may instead need **System Settings > Privacy & Security >
  Open Anyway**, because the app isn't notarised by Apple.
- **Linux:** extract it, then run `./HoldemLab/HoldemLab`.

**New to poker?** Choose **I'm new to poker** when the app first starts. It turns on beginner
help and gives you a short tour. The **Learn** tab explains the rules, hand rankings and poker
terms, and you can press `?` at the table to see which hand beats which.

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
| **Play** | The table. In beginner mode it explains each stage, shows your hand and its strength in plain words, and marks the coach's suggested move. Shortcuts: `F` fold, `C` check/call, `R` bet/raise, `Up`/`Down` change the size, `H` hint, `?` hand rankings, `Space` next hand, `P` pause. The right-hand panel shows your hand, draws, outs, equity against estimated ranges, pot odds and the coach's grade for each decision. |
| **Learn** | How to play, hand rankings with examples, a glossary of poker words and beginner tips. |
| **Review** | Replays recent hands step by step (`Left`/`Right` keys) with every hole card shown and the coach's notes on each of your decisions. |
| **Stats** | Profit graph, decision score by street, a leak finder, your HUD stats against typical winning ranges, results by position, drill scores, and reads on each opponent. |
| **Drills** | Preflop decisions, equity estimation, pot odds and bet math, counting outs, and reading showdowns. |
| **Ranges** | Opening charts for every position, plus a ranking of all 169 starting hands. |
| **Settings** | Beginner mode, number of opponents (1-8), table lineup or per-seat bot styles, cash game or tournament, stakes, bot speed (Relaxed / Normal / Fast), coach mode and training aids. |

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

## Publishing a new version

Bump the version in `CMakeLists.txt`, then push a tag such as `v1.2.0`:

```bash
git tag v1.2.0
```

```bash
git push origin v1.2.0
```

The **Release** workflow builds Windows, macOS (universal) and Linux packages, runs the tests and
publishes them on the Releases page.

## Code layout

```
src/core/    cards, 7-card evaluator, ranges, equity, hand analysis, table engine
src/bots/    HUD stats, Bayesian range model, strategy (preflop + EV postflop), personalities, bot wrapper
src/coach/   spot analysis and decision grading
src/train/   drill generators and graders
src/app/     game controller, JSON persistence, settings, progress, hand history
src/ui/      raylib GUI: widgets, screens, welcome tour and Learn tab
tests/       dependency-free test runner
tools/       headless bot simulator
assets/      DejaVu fonts and the app icon (embedded at build time; see assets/fonts/LICENSE)
```
