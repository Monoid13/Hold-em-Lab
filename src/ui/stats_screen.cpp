#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "bots/profiles.hpp"
#include "train/drills.hpp"
#include "ui/app.hpp"

namespace hl::ui {

namespace {
struct Target {
  const char* key;
  double lo, hi;
};
const Target TARGETS[] = {{"vpip", 0.2, 0.3},    {"pfr", 0.15, 0.25},      {"threeBet", 0.06, 0.11}, {"foldTo3bet", 0.4, 0.6},  {"steal", 0.3, 0.5},
                          {"cbet", 0.5, 0.75},   {"foldToCbet", 0.35, 0.55}, {"foldToBet", 0.35, 0.55}, {"wtsd", 0.24, 0.32}, {"wsd", 0.48, 0.6}};

std::string statTip(const std::string& k, bool low) {
  if (k == "vpip") return low ? "You fold too many playable hands: open wider from late position and defend your big blind." : "You play too many hands. Fold weak offsuit hands, especially out of position.";
  if (k == "pfr") return low ? "Raise more instead of calling; raising wins pots preflop and takes the initiative." : "You raise a very wide range; make sure your opens depend on position.";
  if (k == "threeBet") return low ? "3-bet more: value hands plus a few suited-ace bluffs." : "You 3-bet very often; opponents will 4-bet you light.";
  if (k == "foldTo3bet") return low ? "You defend too much vs 3-bets with dominated hands." : "You fold too often to 3-bets: opponents can 3-bet you with any two cards.";
  if (k == "steal") return low ? "Open more from CO/BTN/SB when folded to you: blinds are free money." : "Your steals are very wide; tighten up vs aggressive blinds.";
  if (k == "cbet") return low ? "C-bet more as the preflop raiser, especially on dry high-card boards." : "You c-bet almost always; check back more on boards that favour the caller.";
  if (k == "foldToCbet") return low ? "You float c-bets too much: fold weak hands that cannot improve." : "You fold to c-bets too often. Defend with pairs, draws and good overcards.";
  if (k == "foldToBet") return low ? "You rarely fold when facing bets: you are paying off value bets." : "You fold too often when bet into; bots will bluff you relentlessly.";
  if (k == "wtsd") return low ? "You give up before showdown too often." : "You go to showdown too much: fold more on the river vs strong lines.";
  return low ? "You lose at showdown: you call down with weak hands." : "You win most showdowns: you may be folding too much earlier.";
}

struct Read {
  std::string label, tip;
};

Read readOpponent(const StatsBook& st, const std::string& id) {
  int n = st.peek(id).hands;
  if (n < 20) return {"Unknown", "Only " + std::to_string(n) + " hands observed."};
  double v = st.raw(id, "vpip").pct.value_or(0), p = st.raw(id, "pfr").pct.value_or(0);
  double af = st.af(id).value_or(0);
  auto fvb = st.raw(id, "foldToBet").pct;
  if (v > 0.42 && p >= 0.27) return {"Maniac", "Call down lighter and let them bluff."};
  if (v > 0.38 && p < 0.16) return {"Calling station", "Value bet thinly; almost never bluff."};
  if (v < 0.16) return {"Nit", "Steal their blinds; fold to their big bets."};
  if (fvb && *fvb > 0.58) return {"Folds too much", "Bluff more, especially on turns and rivers."};
  if (v >= 0.27 && p >= 0.2) return {"LAG", "Widen calling and 3-bet ranges; trap strong hands."};
  if (p / std::max(v, 0.01) >= 0.65) return {"TAG", "Respect raises; attack c-bets when they give up."};
  if (af < 1.3) return {"Passive", "Bet for value; their raises are very strong."};
  return {"Recreational", "Solid value-heavy poker works best."};
}

std::string afStr(std::optional<double> af) {
  if (!af) return "-";
  if (std::isinf(*af)) return "inf";
  char b[16];
  std::snprintf(b, sizeof b, "%.1f", *af);
  return b;
}

// raylib wants triangles in a specific winding; order the vertices so each triangle draws exactly once.
void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
  float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  if (std::fabs(cross) < 1e-3f) return;
  if (cross > 0) std::swap(b, c);
  DrawTriangle(a, b, c, col);
}

double niceStep(double range, double target = 5) {
  double raw = range / target;
  double mag = std::pow(10, std::floor(std::log10(std::max(raw, 1e-9))));
  double norm = raw / mag;
  return (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
}

void profitChart(Gui& g, Rectangle r, const std::vector<HandSummary>& hands) {
  if (hands.size() < 2) {
    g.text("Play a few hands to see your graph.", r.x, r.y + 10, 14, theme::muted);
    return;
  }
  std::vector<double> cum = {0};
  for (auto& h : hands) cum.push_back(cum.back() + h.netBB);
  double mn = std::min(0.0, *std::min_element(cum.begin(), cum.end())), mx = std::max(0.0, *std::max_element(cum.begin(), cum.end()));
  if (mx - mn < 10) {
    mx += 5;
    mn -= 5;
  }
  double step = niceStep(mx - mn);
  mn = std::floor(mn / step) * step;
  mx = std::ceil(mx / step) * step;
  float L = r.x + 52, R = r.x + r.width - 16, T = r.y + 8, B = r.y + r.height - 24;
  size_t n = cum.size() - 1;
  auto X = [&](double i) { return static_cast<float>(L + i / n * (R - L)); };
  auto Y = [&](double v) { return static_cast<float>(T + (mx - v) / (mx - mn) * (B - T)); };
  for (double v = mn; v <= mx + 1e-9; v += step) {
    bool zero = std::fabs(v) < 1e-9;
    DrawLineEx({L, Y(v)}, {R, Y(v)}, 1, zero ? Color{74, 86, 80, 255} : theme::grid);
    g.textRight(std::to_string(static_cast<long>(std::lround(v))), L - 8, Y(v) - 7, 12, theme::muted);
  }
  double xs = niceStep(static_cast<double>(n), 6);
  for (double i = xs; i <= n; i += xs) {
    std::string s = std::to_string(static_cast<long>(i));
    g.text(s, X(i) - g.measure(s, 12).x / 2, B + 6, 12, theme::muted);
  }
  // area wash + line
  for (size_t i = 1; i <= n; ++i) {
    Vector2 a{X(i - 1.0), Y(cum[i - 1])}, b{X(static_cast<double>(i)), Y(cum[i])};
    float z = Y(0);
    tri(a, {a.x, z}, b, withAlpha(theme::series, 0.1f));
    tri(b, {a.x, z}, {b.x, z}, withAlpha(theme::series, 0.1f));
  }
  for (size_t i = 1; i <= n; ++i) DrawLineEx({X(i - 1.0), Y(cum[i - 1])}, {X(static_cast<double>(i)), Y(cum[i])}, 2, theme::series);
  Vector2 last{X(static_cast<double>(n)), Y(cum[n])};
  DrawCircleV(last, 6, theme::surface);
  DrawCircleV(last, 4, theme::series);
  std::string lab = fmtBB(cum[n]);
  g.textRight(lab, std::min(last.x, R - 4), last.y - 22, 13, theme::text, true);
  // hover crosshair
  if (g.hover({L, T, R - L, B - T})) {
    size_t i = static_cast<size_t>(std::clamp(std::lround((g.mouse.x - L) / (R - L) * n), 0L, static_cast<long>(n)));
    DrawLineEx({X(static_cast<double>(i)), T}, {X(static_cast<double>(i)), B}, 1, theme::muted);
    DrawCircleV({X(static_cast<double>(i)), Y(cum[i])}, 6, theme::surface);
    DrawCircleV({X(static_cast<double>(i)), Y(cum[i])}, 4.5f, theme::series);
    std::string tip = "Hand " + std::to_string(i) + ": " + fmtBB(cum[i]) + " total" + (i > 0 ? ", " + fmtBB(hands[i - 1].netBB) + " this hand" : "");
    g.setTooltip(tip);
  }
}

void barRow(Gui& g, float x, float y, float w, const std::string& label, double v, int n) {
  g.textRight(label, x + 64, y + 2, 13, theme::text2);
  Rectangle bg{x + 74, y + 2, w - 74 - 56, 16};
  DrawRectangleRounded(bg, 0.5f, 6, theme::surface2);
  if (v >= 0) DrawRectangleRounded({bg.x, bg.y, std::max(4.f, static_cast<float>(bg.width * v)), bg.height}, 0.5f, 6, theme::series);
  g.text(v >= 0 ? pct(v) : "-", bg.x + bg.width + 8, y + 2, 13, theme::text, true);
  if (g.hover({x, y, w, 20})) g.setTooltip(label + ": " + (v >= 0 ? pct(v) + " decision score over " + std::to_string(n) + " decisions" : "no decisions yet"));
}
}  // namespace

void StatsScreen::draw(App& app, Rectangle area) {
  Gui& g = app.g;
  const Progress& pr = app.progress;
  const StatsBook& st = app.stats;
  Totals T = pr.totals();
  float y0 = scroll_.begin(g, area);
  float y = y0;
  const float W = area.width - 14;
  const float gap = 14;

  // tiles
  struct Tile {
    std::string label, value;
    Color c;
  };
  int errs = T.grades[2] + T.grades[3] + T.grades[4];
  char evl[32], wr[48];
  std::snprintf(evl, sizeof evl, "%.1f bb", pr.evLossBB);
  std::snprintf(wr, sizeof wr, "%s%.1f bb/100", T.bb100 >= 0 ? "+" : "\xE2\x88\x92", std::fabs(T.bb100));
  std::vector<Tile> tiles = {{"Hands played", fmtInt(T.hands), theme::text},
                             {"Net won", fmtBB(T.netBB), T.netBB > 0 ? theme::gain : T.netBB < 0 ? theme::loss : theme::text},
                             {"Win rate", wr, T.bb100 > 0 ? theme::gain : T.bb100 < 0 ? theme::loss : theme::text},
                             {"Decision score", T.accuracy >= 0 ? pct(T.accuracy) : "-", theme::text},
                             {"Errors", fmtInt(errs) + " of " + fmtInt(T.decisions), theme::text},
                             {"EV lost to errors", evl, theme::text}};
  float tw = (W - gap * 5) / 6;
  for (size_t i = 0; i < tiles.size(); ++i) {
    Rectangle r{area.x + i * (tw + gap), y, tw, 70};
    g.panel(r);
    g.text(tiles[i].label, r.x + 12, r.y + 12, 12.5f, theme::muted);
    g.text(g.ellipsize(tiles[i].value, tw - 20, 20, true), r.x + 12, r.y + 34, 20, tiles[i].c, true);
  }
  y += 84;

  // results chart
  Rectangle cr{area.x, y, W, 280};
  g.panel(cr, "Results", "Cumulative winnings in big blinds, hand by hand");
  profitChart(g, {cr.x + 10, cr.y + 44, cr.width - 20, cr.height - 52}, pr.hands);
  y += cr.height + gap;

  float colW = (W - gap) / 2;
  // decision quality
  {
    Rectangle r{area.x, y, colW, 300};
    g.panel(r, "Decision quality by street");
    float yy = r.y + 46;
    static const char* streets[] = {"preflop", "flop", "turn", "river"};
    for (const char* s : streets) {
      auto it = pr.decisions.find(s);
      std::array<int, 5> c = it == pr.decisions.end() ? std::array<int, 5>{} : it->second;
      int n = 0;
      double sc = 0;
      for (int k = 0; k < 5; ++k) {
        n += c[k];
        sc += c[k] * gradeScore(static_cast<GradeLevel>(k));
      }
      barRow(g, r.x + 10, yy, r.width - 20, s, n ? sc / n : -1, n);
      yy += 28;
    }
    yy += 10;
    float cw = (r.width - 28) / 6.f;
    float cx[] = {0, cw, cw * 2, cw * 3, cw * 4, cw * 5};
    g.text("Street", r.x + 14 + cx[0], yy, 12, theme::muted, true);
    static const char* gl[] = {"Best", "Good", "Inaccuracy", "Mistake", "Blunder"};
    for (int k = 0; k < 5; ++k) g.text(gl[k], r.x + 14 + cx[k + 1], yy, 12, theme::muted, true);
    yy += 20;
    for (const char* s : streets) {
      auto it = pr.decisions.find(s);
      std::array<int, 5> c = it == pr.decisions.end() ? std::array<int, 5>{} : it->second;
      g.text(s, r.x + 14, yy, 13, theme::text2);
      for (int k = 0; k < 5; ++k) g.text(std::to_string(c[k]), r.x + 14 + cx[k + 1], yy, 13, theme::text);
      yy += 20;
    }
  }
  // leak finder
  {
    Rectangle r{area.x + colW + gap, y, colW, 300};
    g.panel(r, "Leak finder");
    struct Leak {
      double score;
      std::string title, tip;
    };
    std::vector<Leak> leaks;
    for (auto& t : TARGETS) {
      auto raw = st.raw(HERO_ID, t.key);
      if (raw.n < 25 || !raw.pct) continue;
      double p = *raw.pct;
      if (p < t.lo * 0.85 || p > t.hi * 1.15)
        leaks.push_back({std::fabs(p - (t.lo + t.hi) / 2) * 10, std::string(statDef(t.key).label) + " " + pct(p) + " is " + (p < t.lo ? "low" : "high") + " (typical " + pct(t.lo) + "-" + pct(t.hi) + ")",
                         statTip(t.key, p < t.lo)});
    }
    for (auto& [tag, count] : pr.tags) {
      const LeakTag* lt = leakTag(tag);
      if (count < 2 || !lt) continue;
      leaks.push_back({static_cast<double>(count), lt->label + " - " + std::to_string(count) + "x", lt->tip});
    }
    std::sort(leaks.begin(), leaks.end(), [](const Leak& a, const Leak& b) { return a.score > b.score; });
    float yy = r.y + 44;
    if (leaks.empty()) g.textWrapped("No clear leaks yet. Leaks appear after about 25 hands of a stat, or when the coach flags the same mistake twice.", r.x + 14, yy, r.width - 28, 13.5f, theme::muted);
    g.pushClip({r.x, r.y + 40, r.width, r.height - 44});
    for (size_t i = 0; i < leaks.size() && i < 6; ++i) {
      yy += g.textWrapped(std::to_string(i + 1) + ". " + leaks[i].title, r.x + 14, yy, r.width - 28, 13.5f, theme::text, true);
      yy += g.textWrapped(leaks[i].tip, r.x + 30, yy, r.width - 44, 12.5f, theme::muted) + 6;
    }
    g.popClip();
  }
  y += 300 + gap;

  // HUD stats + positions
  {
    float h = 44 + 22 + 11 * 22 + 10;
    Rectangle r{area.x, y, colW, h};
    g.panel(r, "Your HUD stats", "What the bots see - they adapt to these");
    float yy = r.y + 44;
    float c1 = r.x + 14, c2 = r.x + colW * 0.42f, c3 = r.x + colW * 0.56f, c4 = r.x + colW * 0.68f, c5 = r.x + colW * 0.86f;
    g.text("Stat", c1, yy, 12, theme::muted, true);
    g.text("You", c2, yy, 12, theme::muted, true);
    g.text("n", c3, yy, 12, theme::muted, true);
    g.text("Typical", c4, yy, 12, theme::muted, true);
    yy += 22;
    for (auto& t : TARGETS) {
      auto raw = st.raw(HERO_ID, t.key);
      const StatDef& d = statDef(t.key);
      g.text(d.label, c1, yy, 13, theme::text2);
      if (g.hover({c1, yy, colW * 0.4f, 20})) g.setTooltip(d.desc);
      g.text(raw.pct ? pct(*raw.pct) : "-", c2, yy, 13, theme::text, true);
      g.text(std::to_string(raw.n), c3, yy, 13, theme::muted);
      g.text(pct(t.lo) + "-" + pct(t.hi), c4, yy, 13, theme::muted);
      if (raw.n >= 15 && raw.pct) {
        bool low = *raw.pct < t.lo, high = *raw.pct > t.hi;
        g.text(low ? "\xE2\x96\xBC low" : high ? "\xE2\x96\xB2 high" : "\xE2\x9C\x93 ok", c5, yy, 12.5f, low || high ? theme::warning : theme::gain, true);
      }
      yy += 22;
    }
    g.text("AF", c1, yy, 13, theme::text2);
    g.text(afStr(st.af(HERO_ID)), c2, yy, 13, theme::text, true);
    g.text("2.0-4.0", c4, yy, 13, theme::muted);

    Rectangle pr2{area.x + colW + gap, y, colW, h};
    g.panel(pr2, "By position");
    yy = pr2.y + 44;
    float p1 = pr2.x + 14, p2 = pr2.x + colW * 0.3f, p3 = pr2.x + colW * 0.48f, p4 = pr2.x + colW * 0.64f, p5 = pr2.x + colW * 0.84f;
    g.text("Pos", p1, yy, 12, theme::muted, true);
    g.text("Hands", p2, yy, 12, theme::muted, true);
    g.text("VPIP", p3, yy, 12, theme::muted, true);
    g.text("Net", p4, yy, 12, theme::muted, true);
    g.text("bb/100", p5, yy, 12, theme::muted, true);
    yy += 22;
    static const char* order[] = {"UTG", "UTG+1", "UTG+2", "LJ", "HJ", "CO", "BTN", "SB", "BB"};
    bool any = false;
    for (const char* p : order) {
      auto it = pr.byPosition.find(p);
      if (it == pr.byPosition.end()) continue;
      any = true;
      const PosStat& s = it->second;
      g.text(p, p1, yy, 13, theme::text, true);
      g.text(std::to_string(s.hands), p2, yy, 13, theme::text2);
      g.text(pct(static_cast<double>(s.vpip) / s.hands), p3, yy, 13, theme::text2);
      g.text(fmtBB(s.netBB), p4, yy, 13, s.netBB > 0 ? theme::gain : s.netBB < 0 ? theme::loss : theme::text2);
      g.text(std::to_string(static_cast<long>(std::lround(s.netBB / s.hands * 100))), p5, yy, 13, theme::text2);
      yy += 22;
    }
    if (!any) g.text("No hands yet.", p1, yy, 13, theme::muted);
    y += h + gap;
  }

  // drills
  {
    Rectangle r{area.x, y, W, 44 + 22 + 5 * 22 + 8};
    g.panel(r, "Drills");
    float yy = r.y + 44;
    float d1 = r.x + 14, d2 = r.x + W * 0.4f, d3 = r.x + W * 0.55f, d4 = r.x + W * 0.7f;
    g.text("Drill", d1, yy, 12, theme::muted, true);
    g.text("Tries", d2, yy, 12, theme::muted, true);
    g.text("Correct", d3, yy, 12, theme::muted, true);
    g.text("Best streak", d4, yy, 12, theme::muted, true);
    yy += 22;
    for (int k = 0; k < static_cast<int>(DrillKind::Count); ++k) {
      const DrillInfo& di = drillInfo(static_cast<DrillKind>(k));
      auto it = pr.drills.find(di.key);
      g.text(di.title, d1, yy, 13, theme::text2);
      g.text(it != pr.drills.end() ? std::to_string(it->second.attempts) : "0", d2, yy, 13, theme::text);
      g.text(it != pr.drills.end() && it->second.attempts ? pct(static_cast<double>(it->second.correct) / it->second.attempts) : "-", d3, yy, 13, theme::text);
      g.text(it != pr.drills.end() ? std::to_string(it->second.bestStreak) : "0", d4, yy, 13, theme::text);
      yy += 22;
    }
    y += r.height + gap;
  }

  // opponents
  {
    std::set<std::string> seated;
    if (app.ctrl.table)
      for (auto& p : app.ctrl.table->seats)
        if (p.occupied) seated.insert(p.id);
    std::vector<std::string> ids;
    for (auto& [id, s] : st.players)
      if (id != HERO_ID && s.hands > 0) ids.push_back(id);
    std::sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
      if (seated.count(a) != seated.count(b)) return seated.count(a) > seated.count(b);
      return st.peek(a).hands > st.peek(b).hands;
    });
    if (ids.size() > 16) ids.resize(16);
    float rowH = 38;
    Rectangle r{area.x, y, W, 44 + 24 + std::max<size_t>(1, ids.size()) * rowH + 10};
    g.panel(r, "Opponent reads", "Auto-read from observed stats - compare with your own reads");
    if (g.button({r.x + r.width - 190, r.y + 8, 176, 28}, "Reset opponent stats", Btn::Ghost, !ids.empty(), 12.5f)) {
      for (auto& id : ids) app.stats.reset(id);
      app.ctrl.saveStats();
    }
    float yy = r.y + 46;
    float o[] = {14, 0.2f, 0.28f, 0.35f, 0.42f, 0.5f, 0.6f, 0.86f};
    auto cx = [&](int i) { return i == 0 ? r.x + o[0] : r.x + W * o[i]; };
    const char* heads[] = {"Player", "Hands", "VPIP", "PFR", "AF", "Fold v bet", "Read & exploit", "Actual style"};
    for (int i = 0; i < 8; ++i) g.text(heads[i], cx(i), yy, 12, theme::muted, true);
    yy += 24;
    if (ids.empty()) g.text("Play some hands to build reads.", cx(0), yy, 13, theme::muted);
    for (auto& id : ids) {
      std::string name = id, key;
      size_t a = id.find(':'), b = id.rfind(':');
      if (a != std::string::npos && b != a) {
        name = id.substr(a + 1, b - a - 1);
        key = id.substr(b + 1);
      }
      Color ink = seated.count(id) ? theme::text : theme::muted;
      Read rd = readOpponent(st, id);
      auto rp = [&](const char* k) { return st.raw(id, k).pct ? pct(*st.raw(id, k).pct) : std::string("-"); };
      g.text(name, cx(0), yy, 13.5f, ink, true);
      g.text(std::to_string(st.peek(id).hands), cx(1), yy, 13, ink);
      g.text(rp("vpip"), cx(2), yy, 13, ink);
      g.text(rp("pfr"), cx(3), yy, 13, ink);
      g.text(afStr(st.af(id)), cx(4), yy, 13, ink);
      g.text(rp("foldToBet"), cx(5), yy, 13, ink);
      g.text(rd.label, cx(6), yy - 2, 13, ink, true);
      g.text(g.ellipsize(rd.tip, W * 0.25f, 11.5f), cx(6), yy + 15, 11.5f, theme::muted);
      g.text(app.settings.revealProfiles ? profile(key).label : "hidden", cx(7), yy, 13, app.settings.revealProfiles ? ink : theme::muted);
      if (!app.settings.revealProfiles && g.hover({cx(7), yy, 80, 18})) g.setTooltip("Turn on 'Reveal bot styles' in Settings to check your reads.");
      yy += rowH;
    }
    y += r.height + gap;
  }
  if (g.button({area.x, y, 170, 36}, "Reset my stats", Btn::Danger, true, 14)) {
    app.progress.reset();
    app.stats.reset(HERO_ID);
    app.ctrl.saveStats();
    app.showToast("Your stats were reset (drill scores kept)");
  }
  y += 50;
  scroll_.end(g, y - y0);
}

}  // namespace hl::ui
