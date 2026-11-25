#pragma once
// Bot personalities: biases layered over one EV engine, so weaker profiles make the mistakes real
// players make (calling too much, folding too much, spewing).

#include <string>
#include <vector>

namespace hl {

struct Profile {
  std::string key, label, style, desc, tip;
  double skill = 0.8, adapt = 0.8, looseness = 1, aggression = 0, callBias = 0, foldBias = 0;
  double threeBetMult = 1, bluff3bet = 0.25, trap = 0.1, tiltResist = 0.7;
  bool sizingTell = false;
  double limp = 0, overvalue = 0, chat = 0.1, hollywood = 0, openSize = 0, pfrMult = 0;
};

const std::vector<std::string>& profileKeys();
const Profile& profile(const std::string& key);
const Profile& coachProfile();

struct Lineup {
  std::string key, label;
  std::vector<std::string> seats;
};
const std::vector<Lineup>& lineups();
const Lineup& lineup(const std::string& key);

struct BotName {
  std::string name;
  unsigned int color;  // 0xRRGGBB avatar colour
};
const std::vector<BotName>& botNames();

const std::vector<std::string>& chatLines(const std::string& kind);

}  // namespace hl
