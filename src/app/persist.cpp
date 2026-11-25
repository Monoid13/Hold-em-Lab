#include "app/persist.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace hl {

static const char* GRADE_KEYS[5] = {"best", "good", "inaccuracy", "mistake", "blunder"};
static const char* STREET_KEYS[4] = {"preflop", "flop", "turn", "river"};

static std::string g_dataDirOverride;

void setDataDir(const std::string& path) { g_dataDirOverride = path; }

std::string dataDir() {
  if (const char* o = std::getenv("HOLDEMLAB_DATA_DIR")) return o;
  if (!g_dataDirOverride.empty()) return g_dataDirOverride;
  fs::path p;
#if defined(_WIN32)
  if (const char* a = std::getenv("APPDATA")) p = fs::path(a) / "HoldemLab";
#elif defined(__APPLE__)
  if (const char* h = std::getenv("HOME")) p = fs::path(h) / "Library" / "Application Support" / "HoldemLab";
#else
  if (const char* x = std::getenv("XDG_DATA_HOME")) p = fs::path(x) / "holdemlab";
  else if (const char* h = std::getenv("HOME")) p = fs::path(h) / ".local" / "share" / "holdemlab";
#endif
  if (p.empty()) p = fs::current_path() / "holdemlab-data";
  return p.string();
}

Json loadJson(const std::string& name) {
  std::ifstream in(fs::path(dataDir()) / (name + ".json"), std::ios::binary);
  if (!in) return {};
  std::stringstream ss;
  ss << in.rdbuf();
  return Json::parse(ss.str());
}

bool saveJson(const std::string& name, const Json& j) {
  std::error_code ec;
  fs::create_directories(dataDir(), ec);
  fs::path final = fs::path(dataDir()) / (name + ".json");
  fs::path tmp = final;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << j.dump();
    if (!out) return false;
  }
  fs::rename(tmp, final, ec);
  if (ec) {
    fs::remove(final, ec);
    fs::rename(tmp, final, ec);
  }
  return !ec;
}

void clearAllData() {
  std::error_code ec;
  for (const char* n : {"settings", "progress", "history", "stats"}) fs::remove(fs::path(dataDir()) / (std::string(n) + ".json"), ec);
}

// ---------- settings ----------
double Settings::speedFactor() const {
  if (speed == "instant") return 0.04;
  if (speed == "fast") return 0.45;
  if (speed == "relaxed") return 1.5;
  return 1.0;
}

double Settings::minBotGapMs() const {
  if (speed == "instant") return 0;
  if (speed == "fast") return 500;
  if (speed == "relaxed") return 1600;
  return 1000;
}

Json Settings::toJson() const {
  Json j = Json::object();
  j["heroName"] = heroName;
  j["numOpponents"] = numOpponents;
  j["lineup"] = lineup;
  Json seats = Json::array();
  for (auto& s : customSeats) seats.push(s);
  j["customSeats"] = seats;
  j["startingStackBB"] = startingStackBB;
  j["bigBlind"] = bigBlind;
  j["mode"] = mode;
  j["autoRebuy"] = autoRebuy;
  j["levelHands"] = levelHands;
  j["speed"] = speed;
  j["coachMode"] = coachMode;
  j["showHud"] = showHud;
  j["showOppStats"] = showOppStats;
  j["revealProfiles"] = revealProfiles;
  j["showAllCards"] = showAllCards;
  j["fourColor"] = fourColor;
  j["autoDeal"] = autoDeal;
  j["allInEquity"] = allInEquity;
  j["revealInReview"] = revealInReview;
  j["beginnerMode"] = beginnerMode;
  j["showSuggestion"] = showSuggestion;
  j["seenWelcome"] = seenWelcome;
  return j;
}

Settings Settings::fromJson(const Json& j) {
  Settings s;
  if (!j.isObject()) return s;
  s.heroName = j["heroName"].asStr(s.heroName);
  s.numOpponents = std::clamp(j["numOpponents"].asInt(s.numOpponents), 1, 8);
  s.lineup = j["lineup"].asStr(s.lineup);
  if (j["customSeats"].isArray() && j["customSeats"].size() == 8) {
    s.customSeats.clear();
    for (auto& v : j["customSeats"].arr()) s.customSeats.push_back(v.asStr("tag"));
  }
  s.startingStackBB = std::clamp(j["startingStackBB"].asInt(s.startingStackBB), 10, 500);
  s.bigBlind = std::max(2, j["bigBlind"].asInt(s.bigBlind) / 2 * 2);
  s.mode = j["mode"].asStr(s.mode);
  s.autoRebuy = j["autoRebuy"].asBool(s.autoRebuy);
  s.levelHands = std::clamp(j["levelHands"].asInt(s.levelHands), 3, 50);
  s.speed = j["speed"].asStr(s.speed);
  if (s.speed == "realistic") s.speed = "relaxed";
  s.coachMode = j["coachMode"].asStr(s.coachMode);
  s.showHud = j["showHud"].asBool(s.showHud);
  s.showOppStats = j["showOppStats"].asBool(s.showOppStats);
  s.revealProfiles = j["revealProfiles"].asBool(s.revealProfiles);
  s.showAllCards = j["showAllCards"].asBool(s.showAllCards);
  s.fourColor = j["fourColor"].asBool(s.fourColor);
  s.autoDeal = j["autoDeal"].asBool(s.autoDeal);
  s.allInEquity = j["allInEquity"].asBool(s.allInEquity);
  s.revealInReview = j["revealInReview"].asBool(s.revealInReview);
  s.beginnerMode = j["beginnerMode"].asBool(s.beginnerMode);
  s.showSuggestion = j["showSuggestion"].asBool(s.showSuggestion);
  s.seenWelcome = j["seenWelcome"].asBool(s.seenWelcome);
  return s;
}

Settings Settings::load() { return fromJson(loadJson("settings")); }
void Settings::save() const { saveJson("settings", toJson()); }

const std::vector<std::array<int, 3>>& blindLevels() {
  static const std::vector<std::array<int, 3>> l = {{1, 2, 0},   {2, 4, 0},   {3, 6, 0},   {4, 8, 1},    {5, 10, 1},   {6, 12, 2},   {8, 16, 2},    {10, 20, 3},
                                                    {15, 30, 4}, {20, 40, 5}, {25, 50, 6}, {30, 60, 8}, {40, 80, 10}, {50, 100, 12}, {75, 150, 20}, {100, 200, 25}};
  return l;
}

// ---------- progress ----------
void Progress::load() {
  Json j = loadJson("progress");
  *this = Progress{};
  for (auto& h : j["hands"].arr())
    hands.push_back({h["id"].asInt(), static_cast<long long>(h["t"].asNum()), h["netBB"].asNum(), h["position"].asStr(), h["vpip"].asBool(), h["showdown"].asBool()});
  for (const char* st : STREET_KEYS) {
    std::array<int, 5> c{};
    for (int g = 0; g < 5; ++g) c[g] = j["decisions"][st][GRADE_KEYS[g]].asInt();
    decisions[st] = c;
  }
  for (auto& [k, v] : j["tags"].obj()) tags[k] = v.asInt();
  evLossBB = j["evLossBB"].asNum();
  for (auto& [k, v] : j["drills"].obj()) drills[k] = {v["attempts"].asInt(), v["correct"].asInt(), v["streak"].asInt(), v["bestStreak"].asInt(), static_cast<long>(v["scoreSum"].asNum())};
  for (auto& [k, v] : j["byPosition"].obj()) byPosition[k] = {v["hands"].asInt(), v["vpip"].asInt(), v["netBB"].asNum()};
  sessions = j["sessions"].asInt();
}

void Progress::save() const {
  Json j = Json::object();
  Json hs = Json::array();
  size_t start = hands.size() > 5000 ? hands.size() - 5000 : 0;
  for (size_t i = start; i < hands.size(); ++i) {
    const auto& h = hands[i];
    Json o = Json::object();
    o["id"] = h.id;
    o["t"] = static_cast<double>(h.t);
    o["netBB"] = h.netBB;
    o["position"] = h.position;
    o["vpip"] = h.vpip;
    o["showdown"] = h.showdown;
    hs.push(o);
  }
  j["hands"] = hs;
  Json d = Json::object();
  for (auto& [st, c] : decisions) {
    Json o = Json::object();
    for (int g = 0; g < 5; ++g) o[GRADE_KEYS[g]] = c[g];
    d[st] = o;
  }
  j["decisions"] = d;
  Json t = Json::object();
  for (auto& [k, v] : tags) t[k] = v;
  j["tags"] = t;
  j["evLossBB"] = evLossBB;
  Json dr = Json::object();
  for (auto& [k, v] : drills) {
    Json o = Json::object();
    o["attempts"] = v.attempts;
    o["correct"] = v.correct;
    o["streak"] = v.streak;
    o["bestStreak"] = v.bestStreak;
    o["scoreSum"] = static_cast<double>(v.scoreSum);
    dr[k] = o;
  }
  j["drills"] = dr;
  Json bp = Json::object();
  for (auto& [k, v] : byPosition) {
    Json o = Json::object();
    o["hands"] = v.hands;
    o["vpip"] = v.vpip;
    o["netBB"] = v.netBB;
    bp[k] = o;
  }
  j["byPosition"] = bp;
  j["sessions"] = sessions;
  saveJson("progress", j);
}

void Progress::reset() {
  auto keep = drills;
  int s = sessions;
  *this = Progress{};
  drills = keep;
  sessions = s;
  save();
}

void Progress::recordDecision(const Grade& g) {
  auto& c = decisions[streetName(g.street)];
  c[static_cast<int>(g.grade)]++;
  if (g.hasEvLoss && g.grade != GradeLevel::Best && g.grade != GradeLevel::Good) evLossBB += g.evLossBB;
  for (auto& t : g.tags) tags[t]++;
}

void Progress::recordHand(const HandSummary& h) {
  hands.push_back(h);
  auto& p = byPosition[h.position];
  p.hands++;
  p.netBB += h.netBB;
  if (h.vpip) p.vpip++;
}

DrillStat& Progress::recordDrill(const std::string& key, bool correct, int score) {
  DrillStat& d = drills[key];
  d.attempts++;
  if (correct) {
    d.correct++;
    d.streak++;
    d.bestStreak = std::max(d.bestStreak, d.streak);
  } else
    d.streak = 0;
  d.scoreSum += score;
  save();
  return d;
}

Totals Progress::totals() const {
  Totals t;
  t.hands = static_cast<int>(hands.size());
  for (auto& h : hands) t.netBB += h.netBB;
  t.bb100 = t.hands ? t.netBB / t.hands * 100 : 0;
  for (auto& [st, c] : decisions)
    for (int g = 0; g < 5; ++g) t.grades[g] += c[g];
  for (int g : t.grades) t.decisions += g;
  if (t.decisions) {
    double s = 0;
    for (int g = 0; g < 5; ++g) s += t.grades[g] * gradeScore(static_cast<GradeLevel>(g));
    t.accuracy = s / t.decisions;
  }
  return t;
}

// ---------- history ----------
int Snapshot::potTotal() const {
  int s = pot;
  for (auto& [seat, p] : players) s += p.bet;
  return s;
}

int HandRecord::heroNet() const {
  auto it = nets.find(heroSeat);
  return it == nets.end() ? 0 : it->second;
}

GradeLevel HandRecord::worstGrade(bool& any) const {
  any = !decisions.empty();
  GradeLevel w = GradeLevel::Best;
  for (auto& d : decisions)
    if (static_cast<int>(d.grade) > static_cast<int>(w)) w = d.grade;
  return w;
}

Snapshot snapshot(const Table& t, const std::string& label, int seat) {
  const Hand& h = t.hand;
  Snapshot s;
  s.label = label;
  s.seat = seat;
  s.street = h.street;
  s.board = h.board;
  s.pot = h.pot;
  for (int st : h.order) {
    const HandPlayer& p = h.p(st);
    s.players[st] = {p.stack, p.bet, p.folded, p.allIn};
  }
  return s;
}

static Json cardsJson(const std::vector<Card>& cs) {
  Json a = Json::array();
  for (Card c : cs) a.push(c);
  return a;
}
static std::vector<Card> cardsFrom(const Json& j) {
  std::vector<Card> out;
  for (auto& v : j.arr()) out.push_back(v.asInt());
  return out;
}
static Json strs(const std::vector<std::string>& v) {
  Json a = Json::array();
  for (auto& s : v) a.push(s);
  return a;
}
static std::vector<std::string> strsFrom(const Json& j) {
  std::vector<std::string> out;
  for (auto& v : j.arr()) out.push_back(v.asStr());
  return out;
}
static Json intMap(const std::map<int, int>& m) {
  Json o = Json::object();
  for (auto& [k, v] : m) o[std::to_string(k)] = v;
  return o;
}

static Json recordToJson(const HandRecord& r) {
  Json j = Json::object();
  j["id"] = r.id;
  j["handNo"] = r.handNo;
  j["time"] = static_cast<double>(r.time);
  j["sb"] = r.sb;
  j["bb"] = r.bb;
  j["ante"] = r.ante;
  j["buttonSeat"] = r.buttonSeat;
  j["heroSeat"] = r.heroSeat;
  Json ps = Json::array();
  for (auto& p : r.players) {
    Json o = Json::object();
    o["seat"] = p.seat;
    o["id"] = p.id;
    o["name"] = p.name;
    o["position"] = p.position;
    o["profile"] = p.profile;
    o["startStack"] = p.startStack;
    o["hole"] = cardsJson({p.hole[0], p.hole[1]});
    o["color"] = static_cast<double>(p.color);
    ps.push(o);
  }
  j["players"] = ps;
  Json steps = Json::array();
  for (auto& s : r.steps) {
    Json o = Json::object();
    o["label"] = s.label;
    o["seat"] = s.seat;
    o["street"] = streetIndex(s.street);
    o["board"] = cardsJson(s.board);
    o["pot"] = s.pot;
    o["result"] = s.result;
    Json pl = Json::object();
    for (auto& [seat, p] : s.players) {
      Json q = Json::array();
      q.push(p.stack);
      q.push(p.bet);
      q.push(p.folded);
      q.push(p.allIn);
      pl[std::to_string(seat)] = q;
    }
    o["players"] = pl;
    steps.push(o);
  }
  j["steps"] = steps;
  Json ds = Json::array();
  for (auto& d : r.decisions) {
    Json o = Json::object();
    o["step"] = d.step;
    o["street"] = streetIndex(d.street);
    o["grade"] = static_cast<int>(d.grade);
    o["heroLabel"] = d.heroLabel;
    o["bestLabel"] = d.bestLabel;
    o["hasEvLoss"] = d.hasEvLoss;
    o["evLossBB"] = d.evLossBB;
    o["lines"] = strs(d.lines);
    o["tags"] = strs(d.tags);
    ds.push(o);
  }
  j["decisions"] = ds;
  j["board"] = cardsJson(r.board);
  j["uncontested"] = r.uncontested;
  j["winnings"] = intMap(r.winnings);
  j["nets"] = intMap(r.nets);
  Json sn = Json::object();
  for (auto& [k, v] : r.showdownNames) sn[std::to_string(k)] = v;
  j["showdownNames"] = sn;
  return j;
}

static HandRecord recordFromJson(const Json& j) {
  HandRecord r;
  r.id = j["id"].asStr();
  r.handNo = j["handNo"].asInt();
  r.time = static_cast<long long>(j["time"].asNum());
  r.sb = j["sb"].asInt(1);
  r.bb = j["bb"].asInt(2);
  r.ante = j["ante"].asInt();
  r.buttonSeat = j["buttonSeat"].asInt();
  r.heroSeat = j["heroSeat"].asInt();
  for (auto& o : j["players"].arr()) {
    RecPlayer p;
    p.seat = o["seat"].asInt();
    p.id = o["id"].asStr();
    p.name = o["name"].asStr();
    p.position = o["position"].asStr();
    p.profile = o["profile"].asStr();
    p.startStack = o["startStack"].asInt();
    auto h = cardsFrom(o["hole"]);
    if (h.size() == 2) p.hole = {h[0], h[1]};
    p.color = static_cast<unsigned>(o["color"].asNum());
    r.players.push_back(p);
  }
  for (auto& o : j["steps"].arr()) {
    Snapshot s;
    s.label = o["label"].asStr();
    s.seat = o["seat"].asInt(-1);
    s.street = static_cast<Street>(o["street"].asInt());
    s.board = cardsFrom(o["board"]);
    s.pot = o["pot"].asInt();
    s.result = o["result"].asBool();
    for (auto& [k, q] : o["players"].obj()) {
      const auto& a = q.arr();
      if (a.size() == 4) s.players[std::stoi(k)] = {a[0].asInt(), a[1].asInt(), a[2].asBool(), a[3].asBool()};
    }
    r.steps.push_back(s);
  }
  for (auto& o : j["decisions"].arr()) {
    RecDecision d;
    d.step = o["step"].asInt();
    d.street = static_cast<Street>(o["street"].asInt());
    d.grade = static_cast<GradeLevel>(std::clamp(o["grade"].asInt(1), 0, 4));
    d.heroLabel = o["heroLabel"].asStr();
    d.bestLabel = o["bestLabel"].asStr();
    d.hasEvLoss = o["hasEvLoss"].asBool();
    d.evLossBB = o["evLossBB"].asNum();
    d.lines = strsFrom(o["lines"]);
    d.tags = strsFrom(o["tags"]);
    r.decisions.push_back(d);
  }
  r.board = cardsFrom(j["board"]);
  r.uncontested = j["uncontested"].asBool(true);
  for (auto& [k, v] : j["winnings"].obj()) r.winnings[std::stoi(k)] = v.asInt();
  for (auto& [k, v] : j["nets"].obj()) r.nets[std::stoi(k)] = v.asInt();
  for (auto& [k, v] : j["showdownNames"].obj()) r.showdownNames[std::stoi(k)] = v.asStr();
  return r;
}

void HistoryStore::load() {
  hands.clear();
  Json j = loadJson("history");
  for (auto& o : j.arr()) hands.push_back(recordFromJson(o));
}

void HistoryStore::save() const {
  Json a = Json::array();
  for (auto& r : hands) a.push(recordToJson(r));
  saveJson("history", a);
}

void HistoryStore::add(HandRecord rec) {
  hands.push_back(std::move(rec));
  if (hands.size() > 150) hands.erase(hands.begin(), hands.begin() + (hands.size() - 150));
  save();
}

void HistoryStore::clear() {
  hands.clear();
  save();
}

// ---------- stats ----------
#define HL_STAT_FIELDS(X) \
  X(hands) X(vpip) X(pfr) X(threeBetOpp) X(threeBet) X(foldTo3betOpp) X(foldTo3bet) X(stealOpp) X(steal) X(cbetOpp) X(cbet) \
  X(foldToCbetOpp) X(foldToCbet) X(facingBet) X(foldVsBet) X(postActs) X(aggActs) X(callActs) X(sawFlop) X(wtsd) X(wsd)

Json statsToJson(const StatsBook& s) {
  Json j = Json::object();
  for (auto& [id, p] : s.players) {
    Json o = Json::object();
#define X(f) o[#f] = p.f;
    HL_STAT_FIELDS(X)
#undef X
    o["net"] = static_cast<double>(p.net);
    j[id] = o;
  }
  return j;
}

void statsFromJson(StatsBook& s, const Json& j) {
  s.players.clear();
  for (auto& [id, o] : j.obj()) {
    PlayerStats p;
#define X(f) p.f = o[#f].asInt();
    HL_STAT_FIELDS(X)
#undef X
    p.net = static_cast<long>(o["net"].asNum());
    s.players[id] = p;
  }
}

}  // namespace hl
