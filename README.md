# Hold'em Lab

A no-limit Texas Hold'em trainer for Windows, macOS, Linux and Android. You play against bots that
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
| Android phone or tablet (Android 7+) | `HoldemLab-android.apk` |

- **Windows:** unzip and double-click `HoldemLab.exe`. If SmartScreen says it "protected your
  PC", click **More info**, then **Run anyway**. The app isn't code-signed, which is why Windows
  asks.
- **Mac:** unzip and move `HoldemLab` to Applications. The first time, right-click it and choose
  **Open**. On recent macOS versions you may instead need **System Settings > Privacy & Security >
  Open Anyway**, because the app isn't notarised by Apple.
- **Linux:** extract it, then run `./HoldemLab/HoldemLab`.
- **Android:** open the Releases page on your phone and tap `HoldemLab-android.apk`, then open
  the downloaded file. Android asks you to allow installing apps from your browser (**Settings >
  Install unknown apps**). If Play Protect warns about an unknown app, choose **Install anyway**.
  The game plays in landscape; tap to press buttons and drag to scroll.

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

**Android** (needs JDK 17+, Gradle 8, and the Android SDK with NDK 27 — Android Studio installs
these). Point `ANDROID_HOME` at the SDK, then:

```bash
gradle -p android assembleRelease
```

The APK is `android/app/build/outputs/apk/release/app-release.apk`.

If raylib 5 is already installed system-wide, CMake uses it instead of downloading it. Pass
`-DHOLDEM_BUILD_GUI=OFF` to build only the engine, tests and simulator (no graphics libraries
needed).

## Using the app

| Screen | What it does |
| --- | --- |
| **Play** | The table. In beginner mode it explains each stage, shows your hand and its strength in plain words, and marks the coach's suggested move. Shortcuts: `F` fold, `C` check/call, `R` bet/raise, `Up`/`Down` change the size, `H` hint, `?` hand rankings, `Space` next hand, `P` pause. The right-hand panel shows your hand, draws, outs, equity against estimated ranges, pot odds and the coach's grade for each decision. |
| **Learn** | A 12-chapter poker course - rules, hand rankings, a full example hand, betting rules and side pots, position, starting hands, pot odds and outs, reading the board, bet sizing and bluffing, opponent types, bankroll and tilt, tournaments - with card illustrations, progress ticks and links to drills. Plus a glossary and beginner tips. |
| **Review** | Every recent hand with its result summary, the key moment (your costliest decision), a street-by-street timeline with board and pot, a step-by-step replay (`Left`/`Right` keys) with all hole cards, and the coach's notes on each decision. |
| **Stats** | Filter by all time, last 100 hands or this session. Results graph split into showdown and non-showdown winnings, decision quality per street, a leak finder, a heat map of your results with all 169 starting hands, biggest wins and losses, HUD stats against typical winning ranges, results by position, drill scores and reads on each opponent. |
| **Drills** | Preflop decisions, equity estimation, pot odds and bet math, counting outs, and reading showdowns. |
| **Ranges** | Opening charts for every position, "facing a raise" charts (3-bet / call / fold for any seat vs any raiser), a ranking of all 169 starting hands, and per-hand details with equity vs random and tight ranges. |
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

The **Release** workflow builds Windows, macOS (universal), Linux and Android packages, runs the
tests and publishes them on the Releases page. You can also start it from the **Actions** tab
(**Release > Run workflow**) and type the version.

### Android signing key (optional, recommended)

Without a key, the APK is signed with a throwaway debug key that changes each release, so a phone
has to uninstall the old version before installing a new one. To let updates install over older
versions, create a key once:

```bash
keytool -genkeypair -v -keystore holdemlab.jks -alias holdemlab -keyalg RSA -keysize 4096 -validity 10000
```

Then add four repository secrets (**Settings > Secrets and variables > Actions**):
`ANDROID_KEYSTORE_BASE64` (the output of `base64 -w0 holdemlab.jks`), `ANDROID_KEYSTORE_PASSWORD`,
`ANDROID_KEY_ALIAS` (`holdemlab`) and `ANDROID_KEY_PASSWORD`. Keep `holdemlab.jks` somewhere safe
and never commit it.

## Code layout

```
src/core/    cards, 7-card evaluator, ranges, equity, hand analysis, table engine
src/bots/    HUD stats, Bayesian range model, strategy (preflop + EV postflop), personalities, bot wrapper
src/coach/   spot analysis and decision grading
src/train/   drill generators and graders
src/app/     game controller, JSON persistence, settings, progress, hand history
src/ui/      raylib GUI: widgets, screens, welcome tour and Learn tab (scales for phones and tablets)
android/     Gradle project that packages the same code as an Android app (NativeActivity)
tests/       dependency-free test runner
tools/       headless bot simulator
assets/      DejaVu fonts and the app icon (embedded at build time; see assets/fonts/LICENSE)
```
