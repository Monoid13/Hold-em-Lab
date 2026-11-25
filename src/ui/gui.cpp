#include "ui/gui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

extern "C" {
extern const unsigned char font_regular[];
extern const unsigned int font_regular_size;
extern const unsigned char font_bold[];
extern const unsigned int font_bold_size;
}

namespace hl::ui {

static float g_scale = 1.f;

float uiScale() { return g_scale; }

bool touchUI() {
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
  return true;
#else
  static const bool forced = [] {
    const char* t = std::getenv("HOLDEMLAB_TOUCH");
    return t && *t == '1';
  }();
  return forced;
#endif
}

void updateUiScale() {
  float s = 1.f;
  if (const char* e = std::getenv("HOLDEMLAB_UI_SCALE")) s = static_cast<float>(std::atof(e));
  else if (touchUI()) {
    // Lay phones and tablets out as if the screen were at least ~720 logical pixels tall.
    s = std::min(GetScreenHeight() / 720.f, GetScreenWidth() / 1100.f);
  }
  s = std::clamp(s, 0.5f, 4.f);
  if (touchUI() || std::getenv("HOLDEMLAB_UI_SCALE")) s = std::max(s, 1.f);
  if (s != g_scale || GetFrameTime() == 0) {
    g_scale = s;
    SetMouseScale(1.f / s, 1.f / s);
  }
}

float screenW() { return GetScreenWidth() / g_scale; }
float screenH() { return GetScreenHeight() / g_scale; }

std::string stripKeyHint(const std::string& label) {
  if (!touchUI()) return label;
  auto open = label.rfind('[');
  if (open == std::string::npos || label.back() != ']') return label;
  std::string t = label.substr(0, open);
  while (!t.empty() && t.back() == ' ') t.pop_back();
  return t;
}

Color withAlpha(Color c, float a) { return Color{c.r, c.g, c.b, static_cast<unsigned char>(std::clamp(a, 0.f, 1.f) * 255)}; }
Color hexColor(unsigned rgb) { return Color{static_cast<unsigned char>(rgb >> 16), static_cast<unsigned char>((rgb >> 8) & 255), static_cast<unsigned char>(rgb & 255), 255}; }

std::string fmtInt(long v) {
  std::string s = std::to_string(std::labs(v));
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(i, ",");
  return (v < 0 ? "-" : "") + s;
}

std::string fmtBB(double v, int digits) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%s%.*f bb", v > 0.05 ? "+" : v < -0.05 ? "\xE2\x88\x92" : "", digits, std::fabs(v));
  return buf;
}

std::string pct(double x, int digits) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f%%", digits, x * 100);
  return buf;
}

static Font loadFont(const unsigned char* data, unsigned int size, int px) {
  std::vector<int> cps;
  for (int c = 32; c < 127; ++c) cps.push_back(c);
  for (int c : {0x2660, 0x2663, 0x2665, 0x2666, 0x2605, 0x2713, 0x2026, 0x2212, 0x2192, 0x2190, 0x25B6, 0x25C0, 0x2022, 0x00D7, 0x00B7, 0x2014, 0x2013, 0x25B2, 0x25BC,
                0x2715, 0x00BD, 0x2153, 0x2154, 0x00BC, 0x00BE, 0x221E, 0x2318, 0x23F5})
    cps.push_back(c);
  Font f = LoadFontFromMemory(".ttf", data, static_cast<int>(size), px, cps.data(), static_cast<int>(cps.size()));
  SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
  return f;
}

void Gui::load() {
  reg = loadFont(font_regular, font_regular_size, 22);
  bold = loadFont(font_bold, font_bold_size, 22);
  regLarge = loadFont(font_regular, font_regular_size, 48);
  boldLarge = loadFont(font_bold, font_bold_size, 48);
}

void Gui::unload() {
  UnloadFont(reg);
  UnloadFont(bold);
  UnloadFont(regLarge);
  UnloadFont(boldLarge);
}

void Gui::beginFrame() {
  mouse = GetMousePosition();
  pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  bool rawReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  mouseDelta = (down && !pressed) ? Vector2{mouse.x - prevMouse.x, mouse.y - prevMouse.y} : Vector2{0, 0};
  prevMouse = mouse;
  if (pressed) {
    pressPos = mouse;
    dragging = false;
  }
  float dx = mouse.x - pressPos.x, dy = mouse.y - pressPos.y;
  if (down && !dragging && dx * dx + dy * dy > 14 * 14) dragging = true;
  released = rawReleased && !dragging;  // a finger that moved was scrolling, not tapping
  if (!down && !rawReleased) dragging = false;
  dragClaimed = false;
  wheel = GetMouseWheelMove();
  wheelUsed = false;
  tooltip.clear();
  clips.clear();
  if (pressed) focusField = -1;
}

void Gui::endFrame() {
  if (!tooltip.empty()) {
    float w = std::min(360.f, measure(tooltip, 13).x + 16);
    float h = wrappedHeight(tooltip, w - 16, 13) + 10;
    float x = std::min(mouse.x + 14, screenW() - w - 4);
    float y = std::min(mouse.y + 18, screenH() - h - 4);
    DrawRectangleRounded({x, y, w, h}, 0.2f, 6, theme::surface3);
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.2f, 6, 1, theme::border);
    textWrapped(tooltip, x + 8, y + 5, w - 16, 13, theme::text);
  }
}

static const Font& pick(const Gui& g, float size, bool bold) {
  if (size > 26) return bold ? g.boldLarge : g.regLarge;
  return bold ? g.bold : g.reg;
}

Vector2 Gui::measure(const std::string& s, float size, bool b) const { return MeasureTextEx(pick(*this, size, b), s.c_str(), size, 0); }

void Gui::text(const std::string& s, float x, float y, float size, Color c, bool b) const {
  DrawTextEx(pick(*this, size, b), s.c_str(), {std::round(x), std::round(y)}, size, 0, c);
}

void Gui::textCentered(const std::string& s, Rectangle r, float size, Color c, bool b) const {
  Vector2 m = measure(s, size, b);
  text(s, r.x + (r.width - m.x) / 2, r.y + (r.height - m.y) / 2, size, c, b);
}

void Gui::textRight(const std::string& s, float right, float y, float size, Color c, bool b) const { text(s, right - measure(s, size, b).x, y, size, c, b); }

static std::vector<std::string> wrapLines(const Gui& g, const std::string& s, float width, float size, bool b) {
  std::vector<std::string> lines;
  std::istringstream paras(s);
  std::string para;
  while (std::getline(paras, para)) {
    std::istringstream words(para);
    std::string w, line;
    while (words >> w) {
      // break words that are wider than the box on their own (e.g. long file paths)
      while (g.measure(w, size, b).x > width && w.size() > 1) {
        size_t cut = w.size() - 1;
        while (cut > 1 && g.measure(w.substr(0, cut), size, b).x > width) --cut;
        while (cut > 1 && (static_cast<unsigned char>(w[cut]) & 0xC0) == 0x80) --cut;  // keep UTF-8 intact
        if (!line.empty()) {
          lines.push_back(line);
          line.clear();
        }
        lines.push_back(w.substr(0, cut));
        w = w.substr(cut);
      }
      std::string cand = line.empty() ? w : line + " " + w;
      if (!line.empty() && g.measure(cand, size, b).x > width) {
        lines.push_back(line);
        line = w;
      } else
        line = cand;
    }
    lines.push_back(line);
  }
  return lines;
}

float Gui::textWrapped(const std::string& s, float x, float y, float width, float size, Color c, bool b, float gap) const {
  auto lines = wrapLines(*this, s, width, size, b);
  float yy = y;
  for (auto& l : lines) {
    text(l, x, yy, size, c, b);
    yy += size * gap;
  }
  return yy - y;
}

float Gui::wrappedHeight(const std::string& s, float width, float size, bool b, float gap) const {
  return static_cast<float>(wrapLines(*this, s, width, size, b).size()) * size * gap;
}

std::string Gui::ellipsize(const std::string& s, float maxW, float size, bool b) const {
  if (measure(s, size, b).x <= maxW) return s;
  std::string t = s;
  while (!t.empty() && measure(t + "...", size, b).x > maxW) {
    t.pop_back();
    while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();  // keep UTF-8 intact
    if (!t.empty() && static_cast<unsigned char>(t.back()) >= 0xC0) t.pop_back();
  }
  return t + "...";
}

bool Gui::hover(Rectangle r) const {
  if (!CheckCollisionPointRec(mouse, r)) return false;
  if (modalActive && !CheckCollisionPointRec(mouse, modal)) return false;
  for (auto& c : clips)
    if (!CheckCollisionPointRec(mouse, c)) return false;
  return true;
}

void Gui::pushClip(Rectangle r) {
  if (!clips.empty()) {
    Rectangle p = clips.back();
    float x1 = std::max(r.x, p.x), y1 = std::max(r.y, p.y);
    float x2 = std::min(r.x + r.width, p.x + p.width), y2 = std::min(r.y + r.height, p.y + p.height);
    r = {x1, y1, std::max(0.f, x2 - x1), std::max(0.f, y2 - y1)};
  }
  clips.push_back(r);
  const float s = g_scale;  // scissor works in real screen pixels
  BeginScissorMode(static_cast<int>(r.x * s), static_cast<int>(r.y * s), static_cast<int>(r.width * s + 0.5f), static_cast<int>(r.height * s + 0.5f));
}

void Gui::popClip() {
  EndScissorMode();
  if (!clips.empty()) clips.pop_back();
  if (!clips.empty()) {
    Rectangle r = clips.back();
    const float s = g_scale;
    BeginScissorMode(static_cast<int>(r.x * s), static_cast<int>(r.y * s), static_cast<int>(r.width * s + 0.5f), static_cast<int>(r.height * s + 0.5f));
  }
}

void Gui::setTooltip(const std::string& t) { tooltip = t; }

bool Gui::button(Rectangle r, const std::string& label, Btn style, bool enabled, float size) {
  bool h = enabled && hover(r);
  Color fill = theme::surface3, line = theme::border, ink = theme::text;
  switch (style) {
    case Btn::Primary: fill = theme::accent; line = theme::accent; ink = theme::accentInk; break;
    case Btn::Ghost: fill = withAlpha(theme::surface3, 0); break;
    case Btn::Fold: fill = {58, 35, 35, 255}; line = {90, 48, 48, 255}; break;
    case Btn::Call: fill = {31, 58, 43, 255}; line = {45, 90, 64, 255}; break;
    case Btn::Raise: fill = {59, 49, 21, 255}; line = {107, 85, 32, 255}; ink = {255, 226, 163, 255}; break;
    case Btn::ChipOn: fill = theme::accent; line = theme::accent; ink = theme::accentInk; break;
    case Btn::Danger: fill = withAlpha(theme::surface, 0); line = {107, 43, 43, 255}; ink = {242, 154, 148, 255}; break;
    default: break;
  }
  if (h) fill = style == Btn::Primary || style == Btn::ChipOn ? Color{240, 196, 99, 255} : Color{static_cast<unsigned char>(std::min(255, fill.r + 14)), static_cast<unsigned char>(std::min(255, fill.g + 14)), static_cast<unsigned char>(std::min(255, fill.b + 14)), 255};
  if (style == Btn::Ghost && h) fill = theme::surface2;
  float round = (style == Btn::Chip || style == Btn::ChipOn) ? 1.0f : std::min(0.5f, 18.f / std::max(r.height, 1.f));
  if (!enabled) {
    fill = withAlpha(fill, 0.45f);
    ink = withAlpha(ink, 0.45f);
  }
  if (h && down) r.y += 1;
  DrawRectangleRounded(r, round, 8, fill);
  DrawRectangleRoundedLinesEx(r, round, 8, 1, line);
  textCentered(stripKeyHint(label), r, size, ink, true);
  return h && released;
}

bool Gui::toggle(Rectangle r, const std::string& label, bool& value, const std::string& hint) {
  bool h = hover(r);
  Rectangle box{r.x, r.y + 2, 18, 18};
  DrawRectangleRounded(box, 0.3f, 6, value ? theme::accent : theme::surface2);
  DrawRectangleRoundedLinesEx(box, 0.3f, 6, 1, value ? theme::accent : theme::border);
  if (value) textCentered("\xE2\x9C\x93", box, 14, theme::accentInk, true);
  text(label, r.x + 28, r.y + 2, 15, h ? theme::text : theme::text2);
  if (!hint.empty()) textWrapped(hint, r.x + 28, r.y + 22, r.width - 28, 12.5f, theme::muted);
  if (h && released) {
    value = !value;
    return true;
  }
  return false;
}

bool Gui::slider(Rectangle r, int& value, int min, int max, int step) {
  if (max <= min) return false;
  float t = static_cast<float>(value - min) / (max - min);
  float cy = r.y + r.height / 2;
  DrawRectangleRounded({r.x, cy - 3, r.width, 6}, 1, 6, theme::surface3);
  DrawRectangleRounded({r.x, cy - 3, r.width * t, 6}, 1, 6, theme::accent);
  DrawCircleV({r.x + r.width * t, cy}, 9, theme::accent);
  bool changed = false;
  if (down && hover({r.x - 10, r.y, r.width + 20, r.height})) {
    float nt = std::clamp((mouse.x - r.x) / r.width, 0.f, 1.f);
    int v = min + static_cast<int>(std::lround(nt * (max - min) / step)) * step;
    if (nt >= 0.995f) v = max;
    v = std::clamp(v, min, max);
    if (v != value) {
      value = v;
      changed = true;
    }
  }
  return changed;
}

bool Gui::slider01(Rectangle r, int& value) { return slider(r, value, 0, 100, 1); }

bool Gui::stepper(Rectangle r, const std::string& label, int& value, int min, int max, int step) {
  text(label, r.x, r.y, 14, theme::text2);
  float y = r.y + 22, h = 32;
  bool changed = false;
  if (button({r.x, y, 36, h}, "\xE2\x88\x92", Btn::Normal, value > min, 18)) {
    value = std::max(min, value - step);
    changed = true;
  }
  Rectangle mid{r.x + 40, y, r.width - 80, h};
  DrawRectangleRounded(mid, 0.3f, 6, theme::surface2);
  textCentered(std::to_string(value), mid, 15, theme::text, true);
  if (button({r.x + r.width - 36, y, 36, h}, "+", Btn::Normal, value < max, 18)) {
    value = std::min(max, value + step);
    changed = true;
  }
  return changed;
}

bool Gui::chooser(Rectangle r, const std::string& label, const std::vector<std::string>& options, int& index) {
  text(label, r.x, r.y, 14, theme::text2);
  float y = r.y + 22, h = 32;
  bool changed = false;
  int n = static_cast<int>(options.size());
  if (button({r.x, y, 32, h}, "\xE2\x97\x80", Btn::Normal, true, 12)) {
    index = (index - 1 + n) % n;
    changed = true;
  }
  Rectangle mid{r.x + 36, y, r.width - 72, h};
  DrawRectangleRounded(mid, 0.3f, 6, theme::surface2);
  textCentered(ellipsize(options[std::clamp(index, 0, n - 1)], mid.width - 12, 14), mid, 14, theme::text);
  if (hover(mid) && released) {
    index = (index + 1) % n;
    changed = true;
  }
  if (button({r.x + r.width - 32, y, 32, h}, "\xE2\x96\xB6", Btn::Normal, true, 12)) {
    index = (index + 1) % n;
    changed = true;
  }
  return changed;
}

bool Gui::textField(int id, Rectangle r, std::string& value, int maxLen) {
  bool h = hover(r);
  if (h && pressed) focusField = id;
  bool focused = focusField == id;
  DrawRectangleRounded(r, 0.25f, 6, theme::surface2);
  DrawRectangleRoundedLinesEx(r, 0.25f, 6, 1, focused ? theme::accent : theme::border);
  bool changed = false;
  if (focused) {
    int ch;
    while ((ch = GetCharPressed()) > 0)
      if (ch >= 32 && ch < 127 && static_cast<int>(value.size()) < maxLen) {
        value += static_cast<char>(ch);
        changed = true;
      }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !value.empty()) {
      value.pop_back();
      changed = true;
    }
    if (IsKeyPressed(KEY_ENTER)) focusField = -1;
  }
  std::string shown = value + (focused && static_cast<int>(GetTime() * 2) % 2 == 0 ? "|" : "");
  text(shown, r.x + 10, r.y + (r.height - 15) / 2 - 1, 15, theme::text);
  return changed;
}

void Gui::panel(Rectangle r, const std::string& title, const std::string& subtitle) {
  DrawRectangleRounded(r, std::min(0.5f, 20.f / std::max(1.f, std::min(r.width, r.height))), 8, theme::surface);
  DrawRectangleRoundedLinesEx(r, std::min(0.5f, 20.f / std::max(1.f, std::min(r.width, r.height))), 8, 1, theme::border);
  if (!title.empty()) {
    text(title, r.x + 14, r.y + 12, 16, theme::text, true);
    if (!subtitle.empty()) text(ellipsize(subtitle, r.width - measure(title, 16, true).x - 40, 12.5f), r.x + 24 + measure(title, 16, true).x, r.y + 14, 12.5f, theme::muted);
  }
}

Color Gui::suitColor(int s) const {
  if (fourColor) return s == 0 ? theme::greenSuit : s == 1 ? theme::blueSuit : s == 2 ? theme::red : theme::black;
  return (s == 1 || s == 2) ? theme::red : theme::black;
}

void Gui::card(Card c, Rectangle r, bool faceDown, bool highlight, bool dim) const {
  float round = 0.16f;
  DrawRectangleRounded({r.x + 1, r.y + 3, r.width, r.height}, round, 6, Color{0, 0, 0, 90});
  if (faceDown) {
    DrawRectangleRounded(r, round, 6, Color{242, 238, 228, 255});
    Rectangle in{r.x + 3, r.y + 3, r.width - 6, r.height - 6};
    DrawRectangleRounded(in, round, 6, Color{143, 35, 48, 255});
    Rectangle in2{in.x + 3, in.y + 3, in.width - 6, in.height - 6};
    DrawRectangleRoundedLinesEx(in2, round, 6, 1.5f, Color{196, 72, 88, 255});
    Vector2 c{in.x + in.width / 2, in.y + in.height / 2};
    DrawPoly(c, 4, std::min(in.width, in.height) * 0.28f, 0, Color{196, 72, 88, 255});
    DrawPoly(c, 4, std::min(in.width, in.height) * 0.16f, 0, Color{143, 35, 48, 255});
    return;
  }
  Color face = theme::cardFace;
  DrawRectangleRounded(r, round, 6, face);
  DrawRectangleRoundedLinesEx(r, round, 6, 1, Color{205, 202, 192, 255});
  if (highlight) DrawRectangleRoundedLinesEx({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, round, 6, 3, theme::accent);
  int rank = c >> 2, suit = c & 3;
  Color col = suitColor(suit);
  std::string rk = rankLabel(rank);
  if (r.width >= 40) {
    // classic layout: rank + small suit in the corner, large suit in the middle
    float rs = r.width * 0.36f;
    float rx = r.x + r.width * 0.08f, ry = r.y + r.height * 0.03f;
    text(rk, rx, ry, rs, col, true);
    float cs = r.width * 0.28f;
    float rw = measure(rk, rs, true).x;
    Vector2 cm = measure(suitSymbol(suit), cs);
    text(suitSymbol(suit), rx + (rw - cm.x) / 2, ry + rs * 0.95f, cs, col);
    float bs = r.width * 0.62f;
    Vector2 bm = measure(suitSymbol(suit), bs);
    text(suitSymbol(suit), r.x + r.width * 0.58f - bm.x / 2, r.y + r.height * 0.6f - bm.y / 2, bs, col);
  } else {
    float rs = r.width * 0.42f;
    text(rk, r.x + r.width * 0.09f, r.y + r.height * 0.04f, rs, col, true);
    float ss = r.width * 0.56f;
    Vector2 m = measure(suitSymbol(suit), ss);
    text(suitSymbol(suit), r.x + r.width - m.x - r.width * 0.08f, r.y + r.height - m.y - r.height * 0.02f, ss, col);
  }
  if (dim) DrawRectangleRounded(r, round, 6, Color{14, 19, 17, 130});
}

void Gui::cardSlot(Rectangle r) const {
  DrawRectangleRounded(r, 0.16f, 6, Color{255, 255, 255, 12});
  DrawRectangleRoundedLinesEx(r, 0.16f, 6, 1, Color{255, 255, 255, 40});
}

void Gui::chip(Vector2 c, float rad) const {
  DrawCircleV(c, rad, Color{200, 57, 43, 255});
  DrawRing(c, rad * 0.62f, rad * 0.78f, 0, 360, 24, WHITE);
  DrawCircleV(c, rad * 0.3f, WHITE);
}

void Gui::chipStack(Vector2 base, int amount, int bb, float rad) const {
  // A few chips per stack, colour by size: red, green, blue, black, purple
  static const Color cols[] = {{200, 57, 43, 255}, {36, 140, 76, 255}, {38, 92, 180, 255}, {40, 40, 44, 255}, {120, 64, 170, 255}};
  double x = static_cast<double>(amount) / std::max(1, bb);
  int n = std::clamp(1 + static_cast<int>(std::log2(std::max(1.0, x))), 1, 7);
  for (int i = 0; i < n; ++i) {
    Color c = cols[std::min(4, i / 2 + (x > 50 ? 2 : x > 10 ? 1 : 0))];
    float y = base.y - i * rad * 0.42f;
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(y + 2), rad, rad * 0.5f, Color{0, 0, 0, 120});
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(y), rad, rad * 0.5f, Color{static_cast<unsigned char>(c.r * 0.7f), static_cast<unsigned char>(c.g * 0.7f), static_cast<unsigned char>(c.b * 0.7f), 255});
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(y - 1.5f), rad, rad * 0.5f, c);
    for (int k = 0; k < 4; ++k) {  // edge stripes
      float a = (k * 90 + 30) * DEG2RAD;
      DrawEllipse(static_cast<int>(base.x + std::cos(a) * rad * 0.78f), static_cast<int>(y - 1.5f + std::sin(a) * rad * 0.39f), rad * 0.16f, rad * 0.09f, Color{255, 255, 255, 200});
    }
  }
}

static Color gradeColor(int g) {
  switch (g) {
    case 0:
    case 1: return theme::good;
    case 2: return theme::warning;
    case 3: return theme::serious;
    default: return theme::critical;
  }
}
static const char* gradeSym(int g) {
  static const char* s[] = {"\xE2\x98\x85", "\xE2\x9C\x93", "?!", "?", "??"};
  return s[g];
}
static const char* gradeName(int g) {
  static const char* s[] = {"Best", "Good", "Inaccuracy", "Mistake", "Blunder"};
  return s[g];
}

float Gui::gradeBadgeWidth(int g, float size) const { return std::max(size + 6, measure(gradeSym(g), size * 0.8f, true).x + 8) + measure(gradeName(g), size, true).x + 18; }

void Gui::gradeBadge(float x, float y, int g, float size) const {
  float w = gradeBadgeWidth(g, size);
  float h = size + 10;
  DrawRectangleRounded({x, y, w, h}, 1, 10, theme::surface3);
  float dw = std::max(size + 6, measure(gradeSym(g), size * 0.8f, true).x + 8);
  Rectangle dot{x + 4, y + 3, dw, h - 6};
  DrawRectangleRounded(dot, 1, 10, gradeColor(g));
  textCentered(gradeSym(g), dot, size * 0.8f, (g == 2 || g == 3) ? Color{17, 17, 17, 255} : WHITE, true);
  text(gradeName(g), x + dw + 10, y + 4, size, theme::text, true);
}

float Scroll::begin(Gui& g, Rectangle r) {
  view = r;
  if (!g.wheelUsed && g.wheel != 0 && g.hover(r)) {
    offset -= g.wheel * 48;
    g.wheelUsed = true;
  }
  if (offset < 0) offset = 0;
  g.pushClip(r);
  return r.y - offset;
}

void Scroll::end(Gui& g, float contentHeight) {
  g.popClip();
  float maxOff = std::max(0.f, contentHeight - view.height);
  if (g.dragging && g.down && !g.dragClaimed && maxOff > 0 && CheckCollisionPointRec(g.pressPos, view)) {
    offset -= g.mouseDelta.y;  // finger / mouse drag scrolls the innermost scrollable region
    g.dragClaimed = true;
  }
  offset = std::clamp(offset, 0.f, maxOff);
  if (maxOff > 0) {
    float h = view.height * view.height / contentHeight;
    float y = view.y + (view.height - h) * (offset / maxOff);
    DrawRectangleRounded({view.x + view.width - 5, y, 4, h}, 1, 4, withAlpha(theme::muted, 0.5f));
    Rectangle track{view.x + view.width - 10, view.y, 10, view.height};
    if (g.down && g.hover(track)) offset = std::clamp((g.mouse.y - view.y - h / 2) / (view.height - h) * maxOff, 0.f, maxOff);
  }
}

}  // namespace hl::ui
