// Headless bot-vs-bot simulation.
// Usage: holdem_sim [--hands N] [--runs N] [--seed N] [--seats shark,tag,station,...] [--scale X]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "bots/bot.hpp"
#include "bots/profiles.hpp"
#include "bots/range_model.hpp"
#include "bots/stats.hpp"
#include "core/table.hpp"

using namespace hl;

int main(int argc, char** argv) {
  int hands = 2000, runs = 1;
  uint64_t seed = 1;
  double scale = 1.0;
  std::vector<std::string> seats = {"shark", "tag", "lag", "station", "nit", "maniac"};
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string k = argv[i], v = argv[i + 1];
    if (k == "--hands") hands = std::atoi(v.c_str());
    else if (k == "--runs") runs = std::atoi(v.c_str());
    else if (k == "--seed") seed = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--scale") scale = std::atof(v.c_str());
    else if (k == "--seats") {
      seats.clear();
      std::stringstream ss(v);
      std::string s;
      while (std::getline(ss, s, ',')) seats.push_back(s);
    }
  }
  const int n = static_cast<int>(seats.size());
  const int STACK = 200;
  std::map<std::string, std::vector<double>> totals;
  long decisions = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (int run = 0; run < runs; ++run) {
    Rng rng(seed + run * 1000);
    Table table(n, 1, 2, 0, Rng(seed + run * 1000 + 99));
    StatsBook stats;
    std::vector<BotMemory> mem(n);
    std::vector<long> net(n, 0);
    for (int i = 0; i < n; ++i) table.sit(i, seats[i] + "#" + std::to_string(i), seats[i], STACK);
    for (int h = 0; h < hands; ++h) {
      for (int i = 0; i < n; ++i)
        if (table.seats[i].stack < STACK) {
          net[i] -= STACK - table.seats[i].stack;
          table.seats[i].stack = STACK;
        }
      Hand& hand = table.startHand();
      RangeTracker tr(hand, stats);
      while (!table.hand.finished) {
        int seat = table.hand.actor;
        Decision d = decideBot(table, seat, profile(seats[seat]), &mem[seat], tr, rng, scale);
        tr.observe(table.act(d.action), table.hand);
        ++decisions;
      }
      stats.recordHand(table.hand);
      for (int s : table.hand.order) mem[s].afterHand(table.hand.p(s).net / 2.0, false, profile(seats[s]));
    }
    for (int i = 0; i < n; ++i) net[i] += table.seats[i].stack - STACK;
    if (runs == 1) {
      std::printf("%-12s %9s  %5s %5s %5s %5s %5s %5s %5s\n", "player", "bb/100", "VPIP", "PFR", "3B", "CB", "FvB", "WTSD", "AF");
      for (int i = 0; i < n; ++i) {
        std::string id = seats[i] + "#" + std::to_string(i);
        auto p = [&](const char* s) {
          auto r = stats.raw(id, s);
          return r.pct ? *r.pct * 100 : 0.0;
        };
        auto af = stats.af(id);
        std::printf("%-12s %9.1f  %4.0f%% %4.0f%% %4.0f%% %4.0f%% %4.0f%% %4.0f%% %5.1f\n", id.c_str(), net[i] / 2.0 / hands * 100, p("vpip"), p("pfr"), p("threeBet"), p("cbet"),
                    p("foldToBet"), p("wtsd"), af ? *af : 0.0);
      }
    }
    for (int i = 0; i < n; ++i) totals[seats[i]].push_back(net[i] / 2.0 / hands * 100);
  }
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("\n%d run(s) x %d hands, %ld decisions, %.3f ms/decision\n", runs, hands, decisions, secs * 1000 / decisions);
  if (runs > 1) {
    std::printf("Average bb/100 by profile:\n");
    for (auto& [k, v] : totals) {
      double s = 0;
      for (double x : v) s += x;
      std::printf("  %-10s %8.1f\n", k.c_str(), s / v.size());
    }
  }
  return 0;
}
