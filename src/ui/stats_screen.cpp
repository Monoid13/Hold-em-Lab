#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

#include "bots/profiles.hpp"
#include "core/ranges.hpp"
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

// Categorical slots 1-3 of the reference palette (dark-mode steps).
const Color SERIES_TOTAL{57, 135, 229, 255};     // blue
const Color SERIES_NOSD{217, 89, 38, 255};       // orange
const Color SERIES_SD{25, 158, 112, 255};        // aqua

void resultsChart(Gui& g, Rectangle r, const std::vector<const HandSummary*>& hands) {
  if (hands.size() < 2) {
    g.text("Play a few hands to see your graph.", r.x, r.y + 10, 14, theme::muted);
    return;
  }
  std::vector<double> tot = {0}, sd = {0}, nosd = {0};
  for (auto* h : hands) {
    tot.push_back(tot.back() + h->netBB);
    sd.push_back(sd.back() + (h->showdown ? h->netBB : 0));
    nosd.push_back(nosd.back() + (h->showdown ? 0 : h->netBB));
  }
  double mn = 0, mx = 0;
  for (auto* v : {&tot, &sd, &nosd})
    for (double x : *v) {
      mn = std::min(mn, x);
      mx = std::max(mx, x);
    }
  if (mx - mn < 10) {
    mx += 5;
    mn -= 5;
  }
  double step = niceStep(mx - mn);
  mn = std::floor(mn / step) * step;
  mx = std::ceil(mx / step) * step;
  // legend (always present for multiple series)
  struct S {
    const std::vector<double>* v;
    Color c;
    const char* name;
  } series[] = {{&tot, SERIES_TOTAL, "Total"}, {&sd, SERIES_SD, "At showdown"}, {&nosd, SERIES_NOSD, "Without showdown"}};
  float lx = r.x + 52;
  for (auto& se : series) {
    DrawRectangleRounded({lx, r.y + 4, 16, 4}, 1, 4, se.c);
    std::string t = std::string(se.name) + "  " + fmtBB(se.v->back());
    g.text(t, lx + 22, r.y - 2, 12.5f, theme::text2);
    lx += g.measure(t, 12.5f).x + 48;
  }
  float L = r.x + 52, R = r.x + r.width - 16, T = r.y + 24, B = r.y + r.height - 24;
  size_t n = tot.size() - 1;
  auto X = [&](double i) { return static_cast<float>(L + i / n * (R - L)); };
  auto Y = [&](double v) { return static_cast<float>(T + (mx - v) / (mx - mn) * (B - T)); };
  for (double v = mn; v <= mx + 1e-9; v += step) {
    bool zero = std::fabs(v) < 1e-9;
    DrawLineEx({L, Y(v)}, {R, Y(v)}, 1, zero ? Color{74, 86, 80, 255} : theme::grid);
    g.textRight(std::to_string(static_cast<long>(std::lround(v))), L - 8, Y(v) - 7, 12, theme::muted);
  }
  double xs = niceStep(static_cast<double>(n), 6);
  for (double i = xs; i <= n; i += xs) {
    std::string t = std::to_string(static_cast<long>(i));
    g.text(t, X(i) - g.measure(t, 12).x / 2, B + 6, 12, theme::muted);
  }
  for (size_t i = 1; i <= n; ++i) {  // wash under the total line only
    Vector2 a{X(i - 1.0), Y(tot[i - 1])}, b{X(static_cast<double>(i)), Y(tot[i])};
    float z = Y(0);
    tri(a, {a.x, z}, b, withAlpha(SERIES_TOTAL, 0.1f));
    tri(b, {a.x, z}, {b.x, z}, withAlpha(SERIES_TOTAL, 0.1f));
  }
  for (int k = 2; k >= 0; --k) {
    const auto& v = *series[k].v;
    for (size_t i = 1; i <= n; ++i) DrawLineEx({X(i - 1.0), Y(v[i - 1])}, {X(static_cast<double>(i)), Y(v[i])}, 2, series[k].c);
    Vector2 last{X(static_cast<double>(n)), Y(v[n])};
    DrawCircleV(last, 6, theme::surface);
    DrawCircleV(last, 4, series[k].c);
  }
  if (g.hover({L, T, R - L, B - T})) {
    size_t i = static_cast<size_t>(std::clamp(std::lround((g.mouse.x - L) / (R - L) * n), 0L, static_cast<long>(n)));
    DrawLineEx({X(static_cast<double>(i)), T}, {X(static_cast<double>(i)), B}, 1, theme::muted);
    for (auto& se : series) {
      DrawCircleV({X(static_cast<double>(i)), Y((*se.v)[i])}, 6, theme::surface);
      DrawCircleV({X(static_cast<double>(i)), Y((*se.v)[i])}, 4.5f, se.c);
    }
    g.setTooltip("Hand " + std::to_string(i) + ":  total " + fmtBB(tot[i]) + "  \xC2\xB7  at showdown " + fmtBB(sd[i]) + "  \xC2\xB7  without showdown " + fmtBB(nosd[i]) +
                 (i > 0 ? "  \xC2\xB7  this hand " + fmtBB(hands[i - 1]->netBB) : ""));
  }
}

// Stacked bar of decision grades: good/best, inaccuracy, mistake, blunder (status colours + legend).
void gradeRow(Gui& g, float x, float y, float w, const std::string& label, const std::array<int, 5>& c) {
  int n = 0;
  double sc = 0;
  for (int k = 0; k < 5; ++k) {
    n += c[k];
    sc += c[k] * gradeScore(static_cast<GradeLevel>(k));
  }
  g.textRight(label, x + 64, y + 2, 13, theme::text2);
  Rectangle bg{x + 74, y + 2, w - 74 - 56, 16};
  DrawRectangleRounded(bg, 0.5f, 6, theme::surface2);
  if (n) {
    int parts[4] = {c[0] + c[1], c[2], c[3], c[4]};
    Color cols[4] = {theme::good, theme::warning, theme::serious, theme::critical};
    float px = bg.x;
    for (int k = 0; k < 4; ++k) {
      if (!parts[k]) continue;
      float pw = bg.width * parts[k] / n;
      DrawRectangleRec({px, bg.y, std::max(1.f, pw - 2), bg.height}, cols[k]);  // 2px surface gap
      px += pw;
    }
  }
  g.text(n ? pct(sc / n) : "-", bg.x + bg.width + 8, y + 2, 13, theme::text, true);
  if (g.hover({x, y, w, 20}))
    g.setTooltip(label + ": " + std::to_string(c[0] + c[1]) + " good, " + std::to_string(c[2]) + " inaccuracies, " + std::to_string(c[3]) + " mistakes, " + std::to_string(c[4]) +
                 " blunders (score " + (n ? pct(sc / n) : std::string("-")) + ")");
}

Color diverging(double t) {  // t in [-1, 1]: red (lost) .. neutral grey .. blue (won)
  const Color neutral{56, 56, 53, 255}, pos{57, 135, 229, 255}, neg{230, 103, 103, 255};
  Color pole = t >= 0 ? pos : neg;
  float k = static_cast<float>(std::min(1.0, std::fabs(t)));
  return Color{static_cast<unsigned char>(neutral.r + (pole.r - neutral.r) * k), static_cast<unsigned char>(neutral.g + (pole.g - neutral.g) * k),
               static_cast<unsigned char>(neutral.b + (pole.b - neutral.b) * k), 255};
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

  // ---- period filter ----
  static const char* periods[] = {"All time", "Last 100 hands", "This session"};
  float px = area.x;
  for (int i = 0; i < 3; ++i) {
    float w = g.measure(periods[i], 13, true).x + 24;
    if (g.button({px, y, w, 30}, periods[i], period_ == i ? Btn::ChipOn : Btn::Chip, true, 13)) period_ = i;
    px += w + 8;
  }
  g.text("Results, positions and starting hands use this period; decision quality and HUD stats are all-time.", px + 10, y + 8, 12.5f, theme::muted);
  y += 44;
  std::vector<const HandSummary*> hands;
  for (auto& h : pr.hands)
    if (period_ != 2 || h.t >= sessionStartMs_) hands.push_back(&h);
  if (period_ == 1 && hands.size() > 100) hands.erase(hands.begin(), hands.end() - 100);
  double net = 0, sdNet = 0, nosdNet = 0;
  int won = 0;
  for (auto* h : hands) {
    net += h->netBB;
    (h->showdown ? sdNet : nosdNet) += h->netBB;
    won += h->netBB > 0;
  }
  double bb100 = hands.empty() ? 0 : net / hands.size() * 100;

  // ---- tiles ----
  struct Tile {
    std::string label, value;
    Color c;
    std::string tip;
  };
  int errs = T.grades[2] + T.grades[3] + T.grades[4];
  char evl[32], wr[48];
  std::snprintf(evl, sizeof evl, "%.1f bb", pr.evLossBB);
  std::snprintf(wr, sizeof wr, "%s%.1f bb/100", bb100 >= 0 ? "+" : "\xE2\x88\x92", std::fabs(bb100));
  auto signC = [](double v) { return v > 0 ? theme::gain : v < 0 ? theme::loss : theme::text; };
  std::vector<Tile> tiles = {
      {"Hands played", fmtInt(static_cast<long>(hands.size())), theme::text, std::to_string(won) + " hands won"},
      {"Net won", fmtBB(net), signC(net), "Total winnings in big blinds for this period"},
      {"Win rate", wr, signC(bb100), "Big blinds won per 100 hands. Good players win 5-10 bb/100 against tough opposition."},
      {"At showdown", fmtBB(sdNet), signC(sdNet), "Won or lost in hands that went to showdown"},
      {"Without showdown", fmtBB(nosdNet), signC(nosdNet), "Won or lost in hands decided by a fold. Aggressive winners are positive here."},
      {"Decision score", T.accuracy >= 0 ? pct(T.accuracy) : "-", theme::text, fmtInt(T.decisions) + " graded decisions, " + fmtInt(errs) + " errors (all time)"},
      {"EV lost to errors", evl, theme::text, "Estimated big blinds given away by inaccuracies, mistakes and blunders (all time)"}};
  float tw = (W - gap * (tiles.size() - 1)) / tiles.size();
  for (size_t i = 0; i < tiles.size(); ++i) {
    Rectangle r{area.x + i * (tw + gap), y, tw, 70};
    g.panel(r);
    g.text(g.ellipsize(tiles[i].label, tw - 20, 12.5f), r.x + 12, r.y + 12, 12.5f, theme::muted);
    g.text(g.ellipsize(tiles[i].value, tw - 20, 19, true), r.x + 12, r.y + 34, 19, tiles[i].c, true);
    if (g.hover(r)) g.setTooltip(tiles[i].tip);
  }
  y += 84;

  // ---- results chart ----
  Rectangle cr{area.x, y, W, 300};
  g.panel(cr, "Results", "Cumulative winnings in big blinds, hand by hand");
  resultsChart(g, {cr.x + 10, cr.y + 46, cr.width - 20, cr.height - 54}, hands);
  y += cr.height + gap;

  float colW = (W - gap) / 2;
  // ---- decision quality ----
  {
    Rectangle r{area.x, y, colW, 300};
    g.panel(r, "Decision quality by street", "All time");
    float yy = r.y + 46;
    // legend
    struct Lg {
      Color c;
      const char* t;
    } lg[] = {{theme::good, "Good / best"}, {theme::warning, "Inaccuracy"}, {theme::serious, "Mistake"}, {theme::critical, "Blunder"}};
    float lx = r.x + 84;
    for (auto& l : lg) {
      DrawRectangleRounded({lx, yy + 3, 10, 10}, 0.3f, 4, l.c);
      g.text(l.t, lx + 14, yy, 12, theme::text2);
      lx += g.measure(l.t, 12).x + 30;
    }
    yy += 26;
    static const char* streets[] = {"preflop", "flop", "turn", "river"};
    for (const char* sname : streets) {
      auto it = pr.decisions.find(sname);
      gradeRow(g, r.x + 10, yy, r.width - 20, sname, it == pr.decisions.end() ? std::array<int, 5>{} : it->second);
      yy += 28;
    }
    yy += 10;
    float cw = (r.width - 28) / 6.f;
    float cx[] = {0, cw, cw * 2, cw * 3, cw * 4, cw * 5};
    g.text("Street", r.x + 14 + cx[0], yy, 12, theme::muted, true);
    static const char* gl[] = {"Best", "Good", "Inaccuracy", "Mistake", "Blunder"};
    for (int k = 0; k < 5; ++k) g.text(gl[k], r.x + 14 + cx[k + 1], yy, 12, theme::muted, true);
    yy += 20;
    for (const char* sname : streets) {
      auto it = pr.decisions.find(sname);
      std::array<int, 5> c = it == pr.decisions.end() ? std::array<int, 5>{} : it->second;
      g.text(sname, r.x + 14, yy, 13, theme::text2);
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

  // ---- starting hands heat map + biggest pots ----
  {
    float gridW = std::min(520.f, W * 0.55f);
    float h = 52 + gridW + 40;
    Rectangle r{area.x, y, W, h};
    g.panel(r, "Your starting hands", "Net big blinds won with each starting hand (blue won, red lost)");
    std::array<double, 169> netBy{};
    std::array<int, 169> cnt{};
    double maxAbs = 1;
    for (auto* hs : hands) {
      if (hs->c1 < 0) continue;
      int cls = classOf(hs->c1, hs->c2);
      netBy[cls] += hs->netBB;
      cnt[cls]++;
    }
    for (int i = 0; i < 169; ++i) maxAbs = std::max(maxAbs, std::fabs(netBy[i]));
    float cell = gridW / 13;
    float gx = r.x + 16, gy = r.y + 48;
    for (auto& c : R().classes) {
      Rectangle cr2{gx + c.col * cell, gy + c.row * cell, cell - 2, cell - 2};
      Color fill = cnt[c.index] ? diverging(netBy[c.index] / maxAbs) : theme::surface2;
      DrawRectangleRounded(cr2, 0.15f, 4, fill);
      g.textCentered(c.name, cr2, std::clamp(cell * 0.27f, 8.f, 12.f), cnt[c.index] ? WHITE : theme::muted, cnt[c.index] > 0);
      if (g.hover(cr2))
        g.setTooltip(c.name + ": " + (cnt[c.index] ? std::to_string(cnt[c.index]) + " hands, " + fmtBB(netBy[c.index]) : std::string("not dealt yet")));
    }
    // diverging legend
    float ly = gy + gridW + 8;
    for (int i = 0; i < 40; ++i) DrawRectangleRec({gx + 60 + i * 5.f, ly + 4, 5, 10}, diverging(-1 + i / 19.5));
    g.text("lost", gx + 22, ly + 1, 12, theme::text2);
    g.text("won", gx + 268, ly + 1, 12, theme::text2);
    g.text("grey: not played yet", gx + 310, ly + 1, 12, theme::muted);

    // biggest pots
    float bx = gx + gridW + 30, bw = r.x + r.width - bx - 16;
    std::vector<const HandSummary*> sorted = hands;
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->netBB > b->netBB; });
    auto listPots = [&](float yy, const char* title, bool wins) {
      g.text(title, bx, yy, 14, theme::text, true);
      yy += 26;
      int shown = 0;
      for (int k = 0; k < static_cast<int>(sorted.size()) && shown < 5; ++k) {
        const HandSummary* hs = wins ? sorted[k] : sorted[sorted.size() - 1 - k];
        if (wins ? hs->netBB <= 0 : hs->netBB >= 0) break;
        Rectangle row{bx, yy, bw, 38};
        DrawRectangleRounded(row, 0.2f, 6, theme::surface2);
        if (hs->c1 >= 0) {
          g.card(hs->c1, {row.x + 8, row.y + 4, 21, 30});
          g.card(hs->c2, {row.x + 32, row.y + 4, 21, 30});
        }
        g.text(hs->position, row.x + 64, row.y + 11, 13, theme::text2, true);
        g.text(hs->showdown ? "showdown" : "no showdown", row.x + 110, row.y + 11, 12.5f, theme::muted);
        g.textRight(fmtBB(hs->netBB), row.x + row.width - 12, row.y + 10, 14.5f, wins ? theme::gain : theme::loss, true);
        yy += 44;
        ++shown;
      }
      if (!shown) {
        g.text(wins ? "No winning hands yet." : "No losing hands yet.", bx, yy, 13, theme::muted);
        yy += 24;
      }
      return yy;
    };
    float yy = listPots(r.y + 48, "Biggest wins", true);
    listPots(yy + 12, "Biggest losses", false);
    y += h + gap;
  }

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
    std::map<std::string, PosStat> byPos;
    for (auto* hs : hands) {
      PosStat& ps = byPos[hs->position];
      ps.hands++;
      ps.netBB += hs->netBB;
      ps.vpip += hs->vpip;
    }
    bool any = false;
    for (const char* p : order) {
      auto it = byPos.find(p);
      if (it == byPos.end()) continue;
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
