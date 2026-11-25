#pragma once
// Starting-hand classes (169), combos (1326), hand ranking, range notation and preflop charts.

#include <array>
#include <bitset>
#include <map>
#include <string>
#include <vector>

#include "core/cards.hpp"

namespace hl {

constexpr int NUM_COMBOS = 1326;
constexpr int NUM_CLASSES = 169;

using Range = std::array<float, NUM_COMBOS>;
using ClassSet = std::bitset<NUM_CLASSES>;
using ClassWeights = std::array<float, NUM_CLASSES>;

enum class ClassType { Pair, Suited, Offsuit };

struct HandClass {
  int index;
  std::string name;
  ClassType type;
  int hi, lo, row, col, combos;
};

struct RangeData {
  std::vector<HandClass> classes;          // grid order: row/col 0 = Ace
  std::map<std::string, int> byName;
  std::array<unsigned char, NUM_COMBOS> c1, c2, comboClass;
  std::array<short, 52 * 52> comboIndex;
  std::vector<std::vector<int>> classCombos;
  std::vector<int> ranking;                 // class indices strongest first
  std::array<float, NUM_CLASSES> percentile;  // inclusive fraction of combos at least this strong
  std::array<short, NUM_CLASSES> rankPos;
};
const RangeData& R();

int classOf(Card a, Card b);
inline const std::string& classNameOf(Card a, Card b) { return R().classes[classOf(a, b)].name; }
inline int comboIndex(Card a, Card b) { return R().comboIndex[a * 52 + b]; }

ClassSet parseRange(const std::string& s);
double rangeWidth(const ClassSet& set);

std::vector<std::string> positionLabels(int n);

// raise-first-in charts at 100bb; key "HU" = heads-up button
const std::vector<std::string>& rfiPositions();  // display order
const ClassSet* rfiSet(const std::string& key);
double rfiWidth(const std::string& key);
std::string rfiChartText(const std::string& key);
std::string rfiKey(const std::string& position, int numPlayers);

double softIn(double pct, double width, double softness = 0.025);

double comboWidth(const Range& r);
Range fullRange(float v = 1.f);
ClassWeights combosToClassWeights(const Range& r);

}  // namespace hl
