#include "bots/profiles.hpp"

#include <map>

namespace hl {

static std::vector<Profile> buildProfiles() {
  std::vector<Profile> v;
  Profile p;

  p = Profile{};
  p.key = "shark"; p.label = "Shark"; p.style = "Balanced pro";
  p.desc = "Plays close to equilibrium, mixes bluffs and value, and adapts quickly to your tendencies.";
  p.tip = "Play solid; avoid predictable lines.";
  p.skill = 0.96; p.adapt = 1; p.looseness = 1; p.bluff3bet = 0.3; p.trap = 0.1; p.tiltResist = 0.92; p.chat = 0.08; p.hollywood = 0.25;
  v.push_back(p);

  p = Profile{};
  p.key = "tag"; p.label = "TAG"; p.style = "Tight-aggressive regular";
  p.desc = "Solid and straightforward. Bets when strong, gives up when weak. Slightly too tight.";
  p.tip = "Steal more; attack its c-bets when it gives up.";
  p.skill = 0.82; p.adapt = 0.7; p.looseness = 0.85; p.callBias = -0.01; p.foldBias = 0.02; p.threeBetMult = 0.9; p.bluff3bet = 0.18;
  p.trap = 0.08; p.tiltResist = 0.75; p.chat = 0.08; p.hollywood = 0.1;
  v.push_back(p);

  p = Profile{};
  p.key = "lag"; p.label = "LAG"; p.style = "Loose-aggressive";
  p.desc = "Plays many hands and applies pressure with frequent bets and 3-bets. Hard to read.";
  p.tip = "Widen your calling and 4-bet ranges; trap with strong hands.";
  p.skill = 0.78; p.adapt = 0.8; p.looseness = 1.35; p.aggression = 0.035; p.foldBias = -0.01; p.threeBetMult = 1.4; p.bluff3bet = 0.5;
  p.trap = 0.06; p.tiltResist = 0.55; p.chat = 0.12; p.hollywood = 0.15;
  v.push_back(p);

  p = Profile{};
  p.key = "nit"; p.label = "Nit"; p.style = "Tight-passive rock";
  p.desc = "Waits for premium hands. When a nit raises, believe them.";
  p.tip = "Steal its blinds; fold to its big bets and 3-bets.";
  p.skill = 0.6; p.adapt = 0.3; p.looseness = 0.55; p.aggression = -0.04; p.callBias = -0.02; p.foldBias = 0.07; p.threeBetMult = 0.6;
  p.bluff3bet = 0.03; p.trap = 0.25; p.tiltResist = 0.8; p.chat = 0.05;
  v.push_back(p);

  p = Profile{};
  p.key = "station"; p.label = "Calling Station"; p.style = "Loose-passive fish";
  p.desc = "Calls far too much and rarely folds a pair. Bet sizing leaks hand strength.";
  p.tip = "Value bet thinly and size up; almost never bluff.";
  p.skill = 0.3; p.adapt = 0.1; p.looseness = 1.7; p.aggression = -0.07; p.callBias = 0.16; p.foldBias = -0.06; p.threeBetMult = 0.35;
  p.bluff3bet = 0.02; p.trap = 0.2; p.tiltResist = 0.6; p.sizingTell = true; p.limp = 0.75; p.overvalue = 0.08; p.chat = 0.15;
  p.openSize = 3.5; p.pfrMult = 0.35;
  v.push_back(p);

  p = Profile{};
  p.key = "maniac"; p.label = "Maniac"; p.style = "Hyper-aggressive";
  p.desc = "Raises and bluffs constantly.";
  p.tip = "Call down lighter and let it bluff; avoid bluffing it.";
  p.skill = 0.38; p.adapt = 0.2; p.looseness = 2.0; p.aggression = 0.16; p.callBias = 0.04; p.foldBias = -0.04; p.threeBetMult = 2.2;
  p.bluff3bet = 0.75; p.trap = 0.03; p.tiltResist = 0.2; p.overvalue = 0.03; p.chat = 0.25; p.openSize = 3.5;
  v.push_back(p);

  p = Profile{};
  p.key = "scared"; p.label = "Weak-Tight"; p.style = "Scared recreational";
  p.desc = "Plays a fair number of hands but folds to pressure.";
  p.tip = "Bluff it on scary turns and rivers.";
  p.skill = 0.42; p.adapt = 0.15; p.looseness = 1.15; p.aggression = -0.05; p.callBias = -0.02; p.foldBias = 0.09; p.threeBetMult = 0.5;
  p.bluff3bet = 0.05; p.trap = 0.1; p.tiltResist = 0.5; p.sizingTell = true; p.limp = 0.45; p.chat = 0.1; p.pfrMult = 0.6;
  v.push_back(p);

  return v;
}

static const std::vector<Profile>& all() {
  static const std::vector<Profile> v = buildProfiles();
  return v;
}

const std::vector<std::string>& profileKeys() {
  static const std::vector<std::string> keys = [] {
    std::vector<std::string> k;
    for (auto& p : all()) k.push_back(p.key);
    return k;
  }();
  return keys;
}

const Profile& profile(const std::string& key) {
  for (auto& p : all())
    if (p.key == key) return p;
  return all()[1];
}

const Profile& coachProfile() {
  static const Profile c = [] {
    Profile p = profile("shark");
    p.key = "coach";
    p.skill = 1;
    p.adapt = 1;
    p.aggression = p.callBias = p.foldBias = p.overvalue = 0;
    return p;
  }();
  return c;
}

const std::vector<Lineup>& lineups() {
  static const std::vector<Lineup> l = {
      {"beginner", "Soft table (learn the basics)", {"station", "scared", "station", "nit", "maniac", "scared", "station", "tag"}},
      {"mixed", "Mixed table (realistic)", {"tag", "station", "shark", "scared", "lag", "nit", "maniac", "station"}},
      {"tough", "Tough table (regulars)", {"shark", "lag", "tag", "shark", "tag", "lag", "shark", "nit"}},
      {"sharks", "Shark tank (strongest bots)", {"shark", "shark", "shark", "shark", "shark", "shark", "shark", "shark"}},
  };
  return l;
}

const Lineup& lineup(const std::string& key) {
  for (auto& l : lineups())
    if (l.key == key) return l;
  return lineups()[1];
}

const std::vector<BotName>& botNames() {
  static const std::vector<BotName> n = {
      {"Viktor", 0x4f7cac}, {"Mia", 0xd9822b}, {"Dante", 0x6b5b95}, {"Priya", 0x2a9d8f}, {"Hank", 0x8d6e63}, {"Lena", 0xc0577a},
      {"Omar", 0xc9a227},   {"Greta", 0x5b8c5a}, {"Jules", 0x3d7ea6}, {"Ravi", 0xb5523b}, {"Nora", 0x7a6fd0}, {"Sal", 0x4c8b6f},
      {"Kenji", 0x5f6b7a},  {"Bea", 0xd4a017},  {"Marco", 0xa0522d}, {"Ingrid", 0x3f88c5},
  };
  return n;
}

const std::vector<std::string>& chatLines(const std::string& kind) {
  static const std::map<std::string, std::vector<std::string>> lines = {
      {"winBig", {"nh me", "ship it!", "thanks for the chips", "too easy", "I knew it"}},
      {"badBeat", {"unreal...", "every single time", "how do you call that??", "rigged", "sick river", "ok I'm done being nice"}},
      {"loseBig", {"nice hand", "gg", "wow", "ugh", "should have folded"}},
      {"tiltPlay", {"let's gamble", "I'm coming for you", "all day", "not again"}},
      {"greet", {"gl all", "hi", "let's go", "gl"}},
  };
  static const std::vector<std::string> none;
  auto it = lines.find(kind);
  return it == lines.end() ? none : it->second;
}

}  // namespace hl
