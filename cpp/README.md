# Hold'em Lab: desktop app (C++)

A native desktop version of Hold'em Lab, written in C++17 with [raylib](https://www.raylib.com) for
graphics. It has the same game engine, bots, coach, drills and stats as the browser version. Here
the bots run faster, so they simulate more hands per decision.

The result is a single self-contained executable; the fonts are embedded in it.

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

You need CMake 3.16+, a C++17 compiler and git. The first build downloads raylib automatically.

**Windows** (Visual Studio 2022 with "Desktop development with C++"), in a Developer PowerShell:

```bash
cmake -S cpp -B cpp/build
```

```bash
cmake --build cpp/build --config Release
```

The app is `cpp\build\Release\HoldemLab.exe`.

**macOS** (needs the Xcode command line tools and `brew install cmake`):

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build cpp/build --parallel
```

```bash
./cpp/build/HoldemLab
```

**Linux** (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release && cmake --build cpp/build --parallel
```

```bash
./cpp/build/HoldemLab
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
ctest --test-dir cpp/build -C Release --output-on-failure
```

```bash
./cpp/build/holdem_sim --hands 3000 --runs 4 --seats shark,tag,lag,station,nit,maniac
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
