#pragma once
// Content of the Learn tab: a complete beginner's course in No-Limit Texas Hold'em.

#include <string>
#include <vector>

namespace hl::ui {

/** A piece of chapter content. */
struct Block {
  enum Type { Heading, Para, Bullets, Cards, Tip, Example, Rows, Rankings, Drill };
  Type type;
  std::string a;  // text / title / "label:cards|label:cards" for Cards / drill index for Drill
  std::string b;  // body for Tip/Example/Bullets/Rows ("left|right" lines), label for Drill
};

struct Chapter {
  std::string key, title, summary;
  std::vector<Block> blocks;
};

const std::vector<Chapter>& course();

}  // namespace hl::ui
