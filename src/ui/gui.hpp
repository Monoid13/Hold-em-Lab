#pragma once
// Small immediate-mode GUI on top of raylib: theme, fonts, widgets, cards and scroll areas.

#include <string>
#include <vector>

#include "core/cards.hpp"
#include "raylib.h"

namespace hl::ui {

namespace theme {
inline const Color bg{14, 19, 17, 255};
inline const Color surface{22, 28, 25, 255};
inline const Color surface2{29, 37, 33, 255};
inline const Color surface3{38, 48, 43, 255};
inline const Color border{45, 56, 51, 255};
inline const Color text{237, 241, 238, 255};
inline const Color text2{195, 204, 199, 255};
inline const Color muted{143, 156, 149, 255};
inline const Color accent{230, 180, 80, 255};
inline const Color accentInk{26, 20, 5, 255};
inline const Color felt{23, 99, 63, 255};
inline const Color feltDark{13, 63, 39, 255};
inline const Color rail{59, 42, 29, 255};
inline const Color gain{63, 196, 107, 255};
inline const Color loss{240, 122, 114, 255};
inline const Color series{57, 135, 229, 255};
inline const Color grid{42, 51, 47, 255};
inline const Color good{12, 163, 12, 255};
inline const Color warning{250, 178, 25, 255};
inline const Color serious{236, 131, 90, 255};
inline const Color critical{208, 59, 59, 255};
inline const Color cardFace{251, 251, 248, 255};
inline const Color red{198, 40, 40, 255};
inline const Color blueSuit{31, 95, 207, 255};
inline const Color greenSuit{23, 138, 58, 255};
inline const Color black{27, 27, 27, 255};
}  // namespace theme

enum class Btn { Normal, Primary, Ghost, Fold, Call, Raise, Chip, ChipOn, Danger };

struct Gui {
  Font reg{}, bold{}, regLarge{}, boldLarge{};
  Vector2 mouse{};
  bool pressed = false, released = false, down = false;
  float wheel = 0;
  bool wheelUsed = false;
  bool fourColor = false;
  std::vector<Rectangle> clips;
  std::string tooltip;
  Vector2 tooltipAt{};
  int focusField = -1;  // active text field id
  bool modalActive = false;  // when set, only the modal rectangle receives the mouse
  Rectangle modal{};
  // touch support: a press that moves more than a few pixels is a drag (scroll), not a tap
  Vector2 pressPos{}, prevMouse{}, mouseDelta{};
  bool dragging = false, dragClaimed = false;

  void load();
  void unload();
  void beginFrame();
  void endFrame();

  // text
  Vector2 measure(const std::string& s, float size, bool bold = false) const;
  void text(const std::string& s, float x, float y, float size, Color c, bool bold = false) const;
  void textCentered(const std::string& s, Rectangle r, float size, Color c, bool bold = false) const;
  void textRight(const std::string& s, float right, float y, float size, Color c, bool bold = false) const;
  float textWrapped(const std::string& s, float x, float y, float width, float size, Color c, bool bold = false, float lineGap = 1.3f) const;
  float wrappedHeight(const std::string& s, float width, float size, bool bold = false, float lineGap = 1.3f) const;
  std::string ellipsize(const std::string& s, float maxW, float size, bool bold = false) const;

  // interaction
  bool hover(Rectangle r) const;
  bool clicked(Rectangle r) const { return hover(r) && released; }
  void pushClip(Rectangle r);
  void popClip();

  // widgets
  bool button(Rectangle r, const std::string& label, Btn style = Btn::Normal, bool enabled = true, float size = 15);
  bool toggle(Rectangle r, const std::string& label, bool& value, const std::string& hint = "");
  bool slider(Rectangle r, int& value, int min, int max, int step);
  bool slider01(Rectangle r, int& value);  // 0..100
  bool stepper(Rectangle r, const std::string& label, int& value, int min, int max, int step);
  bool chooser(Rectangle r, const std::string& label, const std::vector<std::string>& options, int& index);
  bool textField(int id, Rectangle r, std::string& value, int maxLen);
  void panel(Rectangle r, const std::string& title = "", const std::string& subtitle = "");
  void setTooltip(const std::string& t);

  // cards & chips
  void card(Card c, Rectangle r, bool faceDown = false, bool highlight = false, bool dim = false) const;
  void cardSlot(Rectangle r) const;
  void chip(Vector2 center, float radius) const;
  void gradeBadge(float x, float y, int grade, float size = 14) const;  // returns nothing; draws symbol + label
  float gradeBadgeWidth(int grade, float size = 14) const;
  Color suitColor(int suit) const;
};

/** Vertical scroll region. Usage: float y = s.begin(g, rect); ...draw at y...; s.end(g, contentHeight). */
struct Scroll {
  float offset = 0;
  Rectangle view{};
  float begin(Gui& g, Rectangle r);
  void end(Gui& g, float contentHeight);
  void toTop() { offset = 0; }
};

// UI scaling: everything is laid out in logical units and scaled to the real screen.
float uiScale();
void updateUiScale();  // call once per frame before drawing
float screenW();       // logical screen size
float screenH();
bool touchUI();        // phones/tablets: no keyboard hints, finger-sized interaction
std::string stripKeyHint(const std::string& label);  // "Fold  [F]" -> "Fold" on touch devices

Color withAlpha(Color c, float a);
Color hexColor(unsigned rgb);
std::string fmtInt(long v);
std::string fmtBB(double v, int digits = 1);
std::string pct(double x, int digits = 0);

}  // namespace hl::ui
