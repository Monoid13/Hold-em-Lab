#pragma once
// Human-readable hand analysis: relative made-hand strength, draws, outs and board texture.

#include <string>
#include <vector>

#include "core/cards.hpp"

namespace hl {

struct DrawInfo {
  bool flushDraw = false, nutFlushDraw = false, backdoorFlush = false, oesd = false, gutshot = false;
  std::vector<std::string> description;
  std::string text() const;
};

struct Outs {
  int drawOuts = 0;     // cards giving a straight or better, using a hole card
  int improveOuts = 0;  // cards improving the hand category, using a hole card
  std::vector<Card> cards;
};

struct Texture {
  bool valid = false, paired = false, monotone = false, twoTone = false, rainbow = false, connected = false;
  double wetness = 0;
  std::string description;
};

struct HandAnalysis {
  int score = 0;
  int category = 0;
  std::string name;    // "Pair of Kings"
  std::string detail;  // "top pair, top kicker"
  DrawInfo draws;
  Outs outs;
  Texture texture;
};

DrawInfo drawInfo(const std::vector<Card>& hole, const std::vector<Card>& board);
Outs countOuts(const std::vector<Card>& hole, const std::vector<Card>& board, const std::vector<Card>& extraDead = {});
Texture boardTexture(const std::vector<Card>& board);
HandAnalysis analyzeHand(const std::vector<Card>& hole, const std::vector<Card>& board);
double outsToEquity(int outs, int cardsToCome, int unseen = 47);

}  // namespace hl
