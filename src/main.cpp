#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

// ---- settings --------------------------------------------------------
static constexpr char const* STAGE_NAMES[] = {"2 ticks", "4 ticks", "8 ticks", "16 ticks", "Off"};
static constexpr int STAGES[] = {2, 4, 8, 16, 0};
static constexpr int STAGE_COUNT = 5;
static int g_stage = 1;

static constexpr int WAVE_MAX_TICKS = 8;

// After a spawn, wave Grid / Min Gap are skipped for this many ticks.
static constexpr int SPAWN_GRACE_TICKS = 16;

// Index: 0 cube, 1 ball, 2 robot, 3 spider, 4 ship, 5 ufo, 6 swing, 7 wave.
// Value: -1 = same as main window, 0 = off, >0 = custom ticks.
static constexpr int MODE_COUNT = 8;
static constexpr int MODE_WAVE = 7;
static constexpr char const* MODE_KEYS[] = {
    "cube-ticks", "ball-ticks", "robot-ticks", "spider-ticks",
    "ship-ticks", "ufo-ticks", "swing-ticks", "wave-ticks"
};
static int g_modeTicks[MODE_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1};

static constexpr char const* WAVE_NAMES[] = {"Min Hold", "Min Gap", "Grid"};
enum WaveMode { WAVE_HOLD = 0, WAVE_GAP = 1, WAVE_GRID = 2 };
static constexpr int WAVE_MODE_COUNT = 3;
static int g_waveMode = WAVE_GRID;

// Experimental: the same three timing styles for ship, UFO and swing.
// Index: 0 ship, 1 ufo, 2 swing. Default is Min Hold (the old behavior).
static constexpr char const* FLY_STYLE_KEYS[] = {"ship-style", "ufo-style", "swing-style"};
static int g_flyStyle[3] = {WAVE_HOLD, WAVE_HOLD, WAVE_HOLD};

// The game speed is typed in as a number. These common speeds are only used
// by the TPS Up / TPS Down hotkeys.
static constexpr int TPS_PRESETS[] = {60, 120, 240, 360, 480, 720, 960, 1000};
static constexpr int TPS_COUNT = 8;
static constexpr int TPS_MIN = 1;
static constexpr int TPS_MAX = 10000;
static int g_tps = 240;

static bool g_debug = false;

// Experimental: buffering during dual-portal sections of one-player levels.
// Off by default. Real 2-player levels are not affected by this.
static bool g_dualEnabled = false;

// Hotkeys are keybind settings. Order: 0 stage, 1 tps up, 2 tps down, 3 debug, 4 wave mode, 5 add section.
static constexpr int KEYBIND_COUNT = 6;
static constexpr char const* KEYBIND_SETTINGS[] = {"kb-stage", "kb-tps-up", "kb-tps-down", "kb-debug", "kb-wave", "kb-section"};

// Raised whenever the level resets (death, restart, checkpoint).
static int g_resetEpoch = 0;
// ----------------------------------------------------------------------

#define DBG(...) do { if (g_debug) log::info(__VA_ARGS__); } while (0)

static int indexOf(char const* const* names, int n, std::string const& v, int def) {
    for (int i = 0; i < n; i++) if (v == names[i]) return i;
    return def;
}

static void loadSettings() {
    auto m = Mod::get();
    g_stage = indexOf(STAGE_NAMES, STAGE_COUNT, m->getSettingValue<std::string>("stage"), 1);
    g_tps = std::clamp(static_cast<int>(m->getSettingValue<int64_t>("tps-value")), TPS_MIN, TPS_MAX);
    g_waveMode = indexOf(WAVE_NAMES, WAVE_MODE_COUNT, m->getSettingValue<std::string>("wave-mode"), WAVE_GRID);
    for (int i = 0; i < MODE_COUNT; i++) {
        g_modeTicks[i] = static_cast<int>(m->getSettingValue<int64_t>(MODE_KEYS[i]));
    }
    for (int i = 0; i < 3; i++) {
        g_flyStyle[i] = indexOf(WAVE_NAMES, WAVE_MODE_COUNT, m->getSettingValue<std::string>(FLY_STYLE_KEYS[i]), WAVE_HOLD);
    }
    g_debug = m->getSettingValue<bool>("debug");
    g_dualEnabled = m->getSettingValue<bool>("dual-mode");
}

static char const* waveModeName() { return WAVE_NAMES[g_waveMode]; }

static float stepsPerTick() {
    return g_tps / 240.0f;
}

static int mainLimitSteps() {
    return static_cast<int>(std::lround(STAGES[g_stage] * stepsPerTick()));
}

static bool isFlyMode(PlayerObject* p) {
    return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing;
}

static bool isGroundMode(PlayerObject* p) {
    return !isFlyMode(p);
}

static int modeIndex(PlayerObject* p) {
    if (p->m_isShip) return 4;
    if (p->m_isBird) return 5;
    if (p->m_isDart) return 7;
    if (p->m_isSwing) return 6;
    if (p->m_isBall) return 1;
    if (p->m_isRobot) return 2;
    if (p->m_isSpider) return 3;
    return 0;
}

// Timing style of the icon's current mode (Min Hold, Min Gap or Grid).
static int styleFor(PlayerObject* p) {
    int idx = modeIndex(p);
    if (idx == MODE_WAVE) return g_waveMode;
    if (idx >= 4 && idx <= 6) return g_flyStyle[idx - 4];
    return WAVE_HOLD;
}

static int ticksForMode(int idx) {
    int c = g_modeTicks[idx];
    if (c < 0) {
        int t = STAGES[g_stage];
        if (idx == MODE_WAVE && t > 0) t = std::min(t, WAVE_MAX_TICKS);
        return t;
    }
    return c;
}

static int modeLimitSteps(PlayerObject* p) {
    int ticks = ticksForMode(modeIndex(p));
    if (ticks <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(ticks * stepsPerTick())));
}

static int waveGridSteps() {
    int ticks = ticksForMode(MODE_WAVE);
    if (ticks <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(ticks * stepsPerTick())));
}

static int spawnGraceSteps() {
    return std::max(1, static_cast<int>(std::lround(SPAWN_GRACE_TICKS * stepsPerTick())));
}

static int ringsTouching(PlayerObject* p) {
    return p->m_touchingRings ? static_cast<int>(p->m_touchingRings->count()) : 0;
}

static char const* modeName(PlayerObject* p) {
    if (p->m_isShip) return "ship";
    if (p->m_isBird) return "ufo";
    if (p->m_isDart) return "wave";
    if (p->m_isSwing) return "swing";
    if (p->m_isBall) return "ball";
    if (p->m_isRobot) return "robot";
    if (p->m_isSpider) return "spider";
    return "cube";
}

// Next common speed above / below the current one. A typed value between two
// presets moves to the neighbor in the direction pressed; at the ends it stays.
static int stepTps(int cur, bool up) {
    if (up) {
        for (int i = 0; i < TPS_COUNT; i++) {
            if (TPS_PRESETS[i] > cur) return TPS_PRESETS[i];
        }
        return cur;
    }
    for (int i = TPS_COUNT - 1; i >= 0; i--) {
        if (TPS_PRESETS[i] < cur) return TPS_PRESETS[i];
    }
    return cur;
}

static void addSectionHere();

static void doHotkey(int action) {
    auto m = Mod::get();
    switch (action) {
        case 0:
            g_stage = (g_stage + 1) % STAGE_COUNT;
            m->setSettingValue<std::string>("stage", STAGE_NAMES[g_stage]);
            log::info("[InputBuffer] Main window -> {} ({} steps, wave {} steps @ {} TPS)",
                      STAGE_NAMES[g_stage], mainLimitSteps(), waveGridSteps(), g_tps);
            break;
        case 1:
            g_tps = stepTps(g_tps, true);
            m->setSettingValue<int64_t>("tps-value", static_cast<int64_t>(g_tps));
            log::info("[InputBuffer] TPS -> {} ({:.2f} steps/tick)", g_tps, stepsPerTick());
            break;
        case 2:
            g_tps = stepTps(g_tps, false);
            m->setSettingValue<int64_t>("tps-value", static_cast<int64_t>(g_tps));
            log::info("[InputBuffer] TPS -> {} ({:.2f} steps/tick)", g_tps, stepsPerTick());
            break;
        case 3:
            g_debug = !g_debug;
            m->setSettingValue<bool>("debug", g_debug);
            log::info("[InputBuffer] Debug logging -> {}", g_debug ? "ON" : "OFF");
            break;
        case 4:
            g_waveMode = (g_waveMode + 1) % WAVE_MODE_COUNT;
            m->setSettingValue<std::string>("wave-mode", WAVE_NAMES[g_waveMode]);
            log::info("[InputBuffer] Wave mode -> {} ({} steps)", waveModeName(), waveGridSteps());
            break;
        case 5:
            addSectionHere();
            break;
    }
}

static void hotkeyPressed(int action, bool down, bool repeat) {
    if (!down || repeat) return;
    if (!PlayLayer::get()) return; // hotkeys only act inside a level
    doHotkey(action);
}

// ---- profiles --------------------------------------------------------
// A profile is a named copy of the timing settings (not hotkeys or debug).
// It is stored as one line of text: key=value|key=value|...
struct ProfileKey { char const* key; char type; }; // s = string, i = int, b = bool
static constexpr ProfileKey PROFILE_KEYS[] = {
    {"stage", 's'}, {"wave-mode", 's'}, {"tps-value", 'i'},
    {"cube-ticks", 'i'}, {"ball-ticks", 'i'}, {"robot-ticks", 'i'}, {"spider-ticks", 'i'},
    {"ship-ticks", 'i'}, {"ufo-ticks", 'i'}, {"swing-ticks", 'i'}, {"wave-ticks", 'i'},
    {"dual-mode", 'b'},
    {"ship-style", 's'}, {"ufo-style", 's'}, {"swing-style", 's'},
};

static std::string serializeProfile() {
    auto m = Mod::get();
    std::string out;
    for (auto const& k : PROFILE_KEYS) {
        if (!out.empty()) out += '|';
        out += k.key;
        out += '=';
        if (k.type == 's') out += m->getSettingValue<std::string>(k.key);
        else if (k.type == 'i') out += std::to_string(m->getSettingValue<int64_t>(k.key));
        else out += m->getSettingValue<bool>(k.key) ? "1" : "0";
    }
    return out;
}

static void applyProfileData(std::string const& data) {
    auto m = Mod::get();
    size_t pos = 0;
    while (pos <= data.size()) {
        size_t end = data.find('|', pos);
        if (end == std::string::npos) end = data.size();
        std::string item = data.substr(pos, end - pos);
        pos = end + 1;
        size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::string key = item.substr(0, eq);
        std::string val = item.substr(eq + 1);
        for (auto const& k : PROFILE_KEYS) {
            if (key != k.key) continue;
            if (k.type == 's') m->setSettingValue<std::string>(k.key, val);
            else if (k.type == 'i') m->setSettingValue<int64_t>(k.key, static_cast<int64_t>(std::strtoll(val.c_str(), nullptr, 10)));
            else m->setSettingValue<bool>(k.key, val == "1");
            break;
        }
    }
    loadSettings();
}

static std::string profileDataKey(std::string const& name) { return "profile-data-" + name; }

static std::vector<std::string> loadProfileNames() {
    std::vector<std::string> out;
    std::string raw = Mod::get()->getSavedValue<std::string>("profile-names", "");
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t end = raw.find('\n', pos);
        if (end == std::string::npos) end = raw.size();
        if (end > pos) out.push_back(raw.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

static void saveProfileNames(std::vector<std::string> const& names) {
    std::string raw;
    for (auto const& n : names) {
        raw += n;
        raw += '\n';
    }
    Mod::get()->setSavedValue<std::string>("profile-names", raw);
}

static bool applyProfileByName(std::string const& name) {
    std::string data = Mod::get()->getSavedValue<std::string>(profileDataKey(name), "");
    if (data.empty()) return false;
    applyProfileData(data);
    return true;
}

// When a level applies its own profile, the player's settings from before are
// saved first and put back when the level is left. The copy is kept on disk,
// so it is also restored the next time the game starts if the game crashed.
static void restoreAfterLevel() {
    std::string snap = Mod::get()->getSavedValue<std::string>("restore-snapshot", "");
    if (snap.empty()) return;
    Mod::get()->setSavedValue<std::string>("restore-snapshot", "");
    applyProfileData(snap);
    log::info("[InputBuffer] Restored your settings from before the level profile");
}

// Which profile a level should use (level id -> profile name).
static std::vector<std::pair<int, std::string>> loadLevelProfiles() {
    std::vector<std::pair<int, std::string>> out;
    std::string raw = Mod::get()->getSavedValue<std::string>("level-profiles", "");
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t end = raw.find('\n', pos);
        if (end == std::string::npos) end = raw.size();
        std::string line = raw.substr(pos, end - pos);
        pos = end + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        out.emplace_back(std::atoi(line.substr(0, eq).c_str()), line.substr(eq + 1));
    }
    return out;
}

static void saveLevelProfiles(std::vector<std::pair<int, std::string>> const& v) {
    std::string raw;
    for (auto const& e : v) {
        raw += std::to_string(e.first);
        raw += '=';
        raw += e.second;
        raw += '\n';
    }
    Mod::get()->setSavedValue<std::string>("level-profiles", raw);
}

static std::string levelProfileFor(int id) {
    for (auto const& e : loadLevelProfiles()) {
        if (e.first == id) return e.second;
    }
    return "";
}

static void setLevelProfile(int id, std::string const& name) {
    auto v = loadLevelProfiles();
    v.erase(std::remove_if(v.begin(), v.end(), [&](auto const& e) { return e.first == id; }), v.end());
    if (!name.empty()) v.emplace_back(id, name);
    saveLevelProfiles(v);
}

static void forgetProfileEverywhere(std::string const& name) {
    auto v = loadLevelProfiles();
    v.erase(std::remove_if(v.begin(), v.end(), [&](auto const& e) { return e.second == name; }), v.end());
    saveLevelProfiles(v);
}

// A key that identifies a level. Uploaded levels use their real ID. Levels
// that were never uploaded (such as copies you made to edit) have ID 0, so
// they get a negative key made from their name instead.
static int levelKeyFor(GJGameLevel* level) {
    if (!level) return 0;
    int id = level->m_levelID.value();
    if (id > 0) return id;
    std::string name = level->m_levelName;
    if (name.empty()) return 0;
    int h = static_cast<int>(std::hash<std::string>{}(name) & 0x3fffffff);
    return -(h + 1);
}

static int currentLevelID() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_level) return 0;
    return levelKeyFor(pl->m_level);
}


// ---- sections (experimental) -------------------------------------------
// A level can be split into sections by percent. Each section has its own
// start and end (two decimals) and its own copy of the settings. Parts of the
// level that no section covers follow the "empty sections" choice. If sections
// overlap, the one that starts last wins.
struct Section { float start; float end; std::string data; };
struct LevelSections {
    int emptyMode = 0; // 0 level's blanket profile, 1 normal settings, 2 buffer off
    std::vector<Section> secs;
};
static constexpr char const* EMPTY_MODE_NAMES[] = {"Blanket profile", "My normal settings", "Buffer off"};
static constexpr int EMPTY_MODE_COUNT = 3;
static constexpr char const* OFF_PROFILE =
    "stage=Off|cube-ticks=0|ball-ticks=0|robot-ticks=0|spider-ticks=0|ship-ticks=0|ufo-ticks=0|swing-ticks=0|wave-ticks=0";

static std::vector<std::string> splitStr(std::string const& s, char sep) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t end = s.find(sep, pos);
        if (end == std::string::npos) end = s.size();
        out.push_back(s.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

static std::string fmtPct(float p) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", p);
    return buf;
}

static float roundPct(float p) {
    return std::round(std::clamp(p, 0.f, 100.f) * 100.f) / 100.f;
}

static void sortSections(LevelSections& ls) {
    std::stable_sort(ls.secs.begin(), ls.secs.end(), [](Section const& a, Section const& b) { return a.start < b.start; });
}

// Read / change one key inside a profile string (key=value|key=value|...).
static std::string profGet(std::string const& data, std::string const& key, std::string const& def = "") {
    for (auto const& item : splitStr(data, '|')) {
        size_t eq = item.find('=');
        if (eq != std::string::npos && item.substr(0, eq) == key) return item.substr(eq + 1);
    }
    return def;
}

static std::string profSet(std::string const& data, std::string const& key, std::string const& val) {
    std::string out;
    bool done = false;
    for (auto const& item : splitStr(data, '|')) {
        if (item.empty()) continue;
        size_t eq = item.find('=');
        std::string k = eq == std::string::npos ? item : item.substr(0, eq);
        if (!out.empty()) out += '|';
        if (k == key) { out += key + "=" + val; done = true; }
        else out += item;
    }
    if (!done) {
        if (!out.empty()) out += '|';
        out += key + "=" + val;
    }
    return out;
}

// Saved as one line per level: id <tab> emptyMode <tab> start,end,data;start,end,data
static LevelSections loadLevelSections(int id) {
    LevelSections ls;
    std::string raw = Mod::get()->getSavedValue<std::string>("level-sections", "");
    for (auto const& line : splitStr(raw, '\n')) {
        auto parts = splitStr(line, '\t');
        if (parts.size() < 3 || std::atoi(parts[0].c_str()) != id) continue;
        ls.emptyMode = std::clamp(std::atoi(parts[1].c_str()), 0, EMPTY_MODE_COUNT - 1);
        for (auto const& item : splitStr(parts[2], ';')) {
            size_t c1 = item.find(',');
            size_t c2 = c1 == std::string::npos ? c1 : item.find(',', c1 + 1);
            if (c2 == std::string::npos) continue;
            float a = roundPct(static_cast<float>(std::atof(item.substr(0, c1).c_str())));
            float b = roundPct(static_cast<float>(std::atof(item.substr(c1 + 1, c2 - c1 - 1).c_str())));
            ls.secs.push_back({a, std::max(a, b), item.substr(c2 + 1)});
        }
        break;
    }
    return ls;
}

static void saveLevelSections(int id, LevelSections const& ls) {
    std::string raw = Mod::get()->getSavedValue<std::string>("level-sections", "");
    std::string out;
    for (auto const& line : splitStr(raw, '\n')) {
        if (line.empty()) continue;
        auto parts = splitStr(line, '\t');
        if (!parts.empty() && std::atoi(parts[0].c_str()) == id) continue;
        out += line;
        out += '\n';
    }
    if (!ls.secs.empty() || ls.emptyMode != 0) {
        out += std::to_string(id) + '\t' + std::to_string(ls.emptyMode) + '\t';
        for (size_t i = 0; i < ls.secs.size(); i++) {
            if (i) out += ';';
            out += fmtPct(ls.secs[i].start) + ',' + fmtPct(ls.secs[i].end) + ',' + ls.secs[i].data;
        }
        out += '\n';
    }
    Mod::get()->setSavedValue<std::string>("level-sections", out);
}

// Runtime state for the level being played.
static bool g_secActive = false;
static LevelSections g_sec;      // sorted copy
static std::string g_secToken;   // what is currently applied
static float g_lastDeathPct = -1.f;

static std::string snapshotData() {
    return Mod::get()->getSavedValue<std::string>("restore-snapshot", "");
}

static void ensureSnapshot() {
    if (snapshotData().empty()) {
        Mod::get()->setSavedValue<std::string>("restore-snapshot", serializeProfile());
    }
}

static std::string emptyToken() {
    return g_sec.emptyMode == 1 ? "N" : (g_sec.emptyMode == 2 ? "O" : "B");
}

// Which section covers this percent. The one that starts last wins.
static std::string sectionToken(float pct) {
    int idx = -1;
    for (size_t i = 0; i < g_sec.secs.size(); i++) {
        auto const& s = g_sec.secs[i];
        if (s.data.empty()) continue;
        bool inside = pct + 0.0001f >= s.start && (pct < s.end - 0.0001f || s.end >= 99.995f);
        if (inside) idx = static_cast<int>(i);
    }
    return idx >= 0 ? "S:" + std::to_string(idx) : emptyToken();
}

static void applyToken(std::string const& tok, int levelID) {
    if (tok == "O") { applyProfileData(OFF_PROFILE); return; }
    if (tok.rfind("S:", 0) == 0) {
        size_t i = static_cast<size_t>(std::atoi(tok.c_str() + 2));
        if (i < g_sec.secs.size()) applyProfileData(g_sec.secs[i].data);
        return;
    }
    if (tok == "B") {
        std::string name = levelProfileFor(levelID);
        if (!name.empty() && applyProfileByName(name)) return;
    }
    std::string snap = snapshotData();
    if (!snap.empty()) applyProfileData(snap);
}

// Call after the sections of the current level were edited.
static void reloadSectionsRuntime() {
    int id = currentLevelID();
    if (id == 0) return;
    bool wasActive = g_secActive;
    LevelSections ls = loadLevelSections(id);
    sortSections(ls);
    bool on = Mod::get()->getSettingValue<bool>("sections") && !ls.secs.empty();
    g_sec = ls;
    g_secActive = on;
    g_secToken.clear();
    if (on) ensureSnapshot();
    else if (wasActive) applyToken("B", id);
}

// A new section starts at `start`, ends where the next one starts (or at the
// end of the level), and begins as a copy of the current settings.
static Section newSection(LevelSections const& ls, float start) {
    float end = 100.f;
    for (auto const& s : ls.secs) {
        if (s.start > start + 0.005f) end = std::min(end, s.start);
    }
    return {start, end, serializeProfile()};
}

static void addSectionHere() {
    auto pl = PlayLayer::get();
    int id = currentLevelID();
    if (!pl || id == 0) return;
    float pct = roundPct(pl->getCurrentPercent());
    LevelSections ls = loadLevelSections(id);
    for (auto const& s : ls.secs) {
        if (std::fabs(s.start - pct) < 0.005f) return;
    }
    ls.secs.push_back(newSection(ls, pct));
    sortSections(ls);
    saveLevelSections(id, ls);
    reloadSectionsRuntime();
    std::string msg = "Section added at " + fmtPct(pct) + "%";
    Notification::create(msg.c_str(), NotificationIcon::Success, 1.5f)->show();
}

// Small window that asks where a new section should start.
class AddSectionPopup : public geode::Popup {
protected:
    std::function<void(float)> m_pick;
    TextInput* m_input = nullptr;
    CCLabelBMFont* m_status = nullptr;

    bool init(std::function<void(float)> pick) {
        if (!Popup::init(300.f, 190.f)) return false;
        m_pick = std::move(pick);
        this->setTitle("Add a section");

        auto label = CCLabelBMFont::create("Where should it start?", "bigFont.fnt");
        label->setScale(0.4f);
        m_mainLayer->addChildAtPosition(label, Anchor::Center, {0.f, 50.f});

        m_input = TextInput::create(100.f, "Type %");
        m_input->setFilter("0123456789.");
        m_input->setMaxCharCount(6);
        m_input->setScale(0.8f);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {-50.f, 24.f});
        button("Add", 70.f, 24.f, menu_selector(AddSectionPopup::onTyped));

        std::string death = g_lastDeathPct >= 0.f
            ? "Last death: " + fmtPct(roundPct(g_lastDeathPct)) + "%"
            : "Last death: none yet";
        button(death.c_str(), 0.f, -10.f, menu_selector(AddSectionPopup::onDeath));

        auto pl = PlayLayer::get();
        std::string now = pl
            ? "Current position: " + fmtPct(roundPct(pl->getCurrentPercent())) + "%"
            : "Current position: unavailable";
        button(now.c_str(), 0.f, -42.f, menu_selector(AddSectionPopup::onNow));

        m_status = CCLabelBMFont::create("", "bigFont.fnt");
        m_status->setScale(0.3f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Center, {0.f, -70.f});
        return true;
    }

    void button(char const* s, float x, float y, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create(s);
        spr->setScale(0.55f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
    }

    void choose(float pct) {
        auto pick = m_pick;
        this->onClose(nullptr);
        if (pick) pick(roundPct(pct));
    }

public:
    static AddSectionPopup* create(std::function<void(float)> pick) {
        auto ret = new AddSectionPopup();
        if (ret->init(std::move(pick))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void onTyped(CCObject*) {
        std::string s = m_input->getString();
        if (s.empty()) { m_status->setString("Type a percentage first."); return; }
        float v = static_cast<float>(std::atof(s.c_str()));
        if (v < 0.f || v > 100.f) { m_status->setString("Use a number from 0 to 100."); return; }
        choose(v);
    }

    void onDeath(CCObject*) {
        if (g_lastDeathPct < 0.f) { m_status->setString("You haven't died in this level yet."); return; }
        choose(g_lastDeathPct);
    }

    void onNow(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl) { m_status->setString("Open this from inside a level."); return; }
        choose(pl->getCurrentPercent());
    }
};

// The window where sections are edited. Every section keeps its own copy of
// the settings, which can be changed here without leaving the window.
class SectionsPopup : public geode::Popup {
protected:
    int m_levelID = 0;
    LevelSections m_ls;
    int m_idx = 0;
    int m_page = 0; // 0 window, 1 modes, 2 styles, 3 more
    bool m_updating = false;
    std::vector<std::string> m_names;
    int m_copySel = 0;
    std::vector<CCNode*> m_pageNodes;
    TextInput* m_startInput = nullptr;
    TextInput* m_endInput = nullptr;
    TextInput* m_copyInput = nullptr;
    CCLabelBMFont* m_idxLabel = nullptr;
    CCLabelBMFont* m_nameLabel = nullptr;
    CCLabelBMFont* m_emptyLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;

    CCLabelBMFont* text(char const* s, float x, float y, float scale, bool leftAlign = false, bool onPage = false) {
        auto label = CCLabelBMFont::create(s, "bigFont.fnt");
        label->setScale(scale);
        if (leftAlign) label->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(label, Anchor::Center, {x, y});
        if (onPage) m_pageNodes.push_back(label);
        return label;
    }

    CCMenuItemSpriteExtra* button(char const* s, float x, float y, SEL_MenuHandler cb, float scale = 0.6f) {
        auto spr = ButtonSprite::create(s);
        spr->setScale(scale);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        return btn;
    }

    CCMenuItemSpriteExtra* arrow(bool flip, float x, float y, SEL_MenuHandler cb, int tag = 0, bool onPage = false) {
        auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        spr->setScale(0.5f);
        spr->setFlipX(flip);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        btn->setTag(tag);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        if (onPage) m_pageNodes.push_back(btn);
        return btn;
    }

    TextInput* input(float w, char const* hint, char const* filter, int maxChars, float x, float y, bool onPage) {
        auto in = TextInput::create(w, hint);
        in->setFilter(filter);
        in->setMaxCharCount(maxChars);
        m_mainLayer->addChildAtPosition(in, Anchor::Center, {x, y});
        if (onPage) m_pageNodes.push_back(in);
        return in;
    }

    bool init(int levelID) {
        if (!Popup::init(400.f, 290.f)) return false;
        m_levelID = levelID;
        bool sectionsOn = Mod::get()->getSettingValue<bool>("sections");
        this->setTitle(sectionsOn ? "Sections (Experimental)" : "Sections are OFF");
        m_names = loadProfileNames();
        m_ls = loadLevelSections(levelID);
        sortSections(m_ls);

        m_idxLabel = text("", 0.f, 106.f, 0.45f);
        arrow(false, -95.f, 106.f, menu_selector(SectionsPopup::onPrev));
        arrow(true, 95.f, 106.f, menu_selector(SectionsPopup::onNext));

        text("Starts at %", -190.f, 80.f, 0.35f, true);
        m_startInput = input(70.f, "0.00", "0123456789.", 6, -95.f, 80.f, false);
        m_startInput->setCallback([this](std::string const& s) {
            if (m_updating || m_ls.secs.empty()) return;
            auto& sec = m_ls.secs[m_idx];
            sec.start = roundPct(static_cast<float>(std::atof(s.c_str())));
            if (sec.end < sec.start) sec.end = sec.start;
            this->commit();
        });
        m_startInput->setScale(0.8f);
        text("Ends at %", 0.f, 80.f, 0.35f, true);
        m_endInput = input(70.f, "100.00", "0123456789.", 6, 95.f, 80.f, false);
        m_endInput->setCallback([this](std::string const& s) {
            if (m_updating || m_ls.secs.empty()) return;
            auto& sec = m_ls.secs[m_idx];
            sec.end = std::max(sec.start, roundPct(static_cast<float>(std::atof(s.c_str()))));
            this->commit();
        });
        m_endInput->setScale(0.8f);

        button("Window", -135.f, 54.f, menu_selector(SectionsPopup::onPageWindow), 0.5f);
        button("Modes", -45.f, 54.f, menu_selector(SectionsPopup::onPageModes), 0.5f);
        button("Styles", 40.f, 54.f, menu_selector(SectionsPopup::onPageStyles), 0.5f);
        button("More", 125.f, 54.f, menu_selector(SectionsPopup::onPageMore), 0.5f);

        button("Add", -165.f, 106.f, menu_selector(SectionsPopup::onAdd), 0.5f);
        button("Delete", 165.f, 106.f, menu_selector(SectionsPopup::onDelete), 0.5f);
        m_status = text("", 0.f, 32.f, 0.27f);


        this->refresh();
        this->defaultStatus();
        this->buildPage();
        return true;
    }

    // Saves the edits and updates the running level.
    void commit() {
        saveLevelSections(m_levelID, m_ls);
        reloadSectionsRuntime();
    }

    Section* current() {
        if (m_ls.secs.empty() || m_idx < 0 || m_idx >= static_cast<int>(m_ls.secs.size())) return nullptr;
        return &m_ls.secs[m_idx];
    }

    // Name and short label for the cycling rows (tag = row * 2 + direction).
    void cycleRow(char const* label, int row, std::string const& value, float y) {
        text(label, -190.f, y, 0.4f, true, true);
        arrow(false, -10.f, y, menu_selector(SectionsPopup::onCycle), row * 2, true);
        text(value.c_str(), 70.f, y, 0.4f, false, true);
        arrow(true, 150.f, y, menu_selector(SectionsPopup::onCycle), row * 2 + 1, true);
    }

    void tickInput(char const* label, char const* key, float lx, float ix, float y, std::string const& val, bool isTps) {
        text(label, lx, y, 0.4f, true, true);
        auto in = input(80.f, "-1", isTps ? "0123456789" : "-0123456789", isTps ? 5 : 4, ix, y, true);
        in->setScale(0.7f);
        in->setString(val);
        std::string k = key;
        in->setCallback([this, k, isTps](std::string const& s) {
            if (m_updating) return;
            auto sec = current();
            if (!sec || s.empty() || s == "-") return;
            int v = std::atoi(s.c_str());
            v = isTps ? std::clamp(v, TPS_MIN, TPS_MAX) : std::clamp(v, -1, 1000);
            sec->data = profSet(sec->data, k, std::to_string(v));
            this->commit();
        });
    }

    // Rebuilds the settings area for the current section and page.
    void buildPage() {
        m_updating = true;
        for (auto n : m_pageNodes) n->removeFromParent();
        m_pageNodes.clear();
        m_nameLabel = nullptr;
        m_emptyLabel = nullptr;
        m_copyInput = nullptr;
        if (m_page == 3) {
            buildMore();
            m_updating = false;
            return;
        }
        auto sec = current();
        if (m_levelID == 0 || !sec) {
            text("Add a section to edit its settings.", 0.f, 0.f, 0.4f, false, true);
            m_updating = false;
            return;
        }
        std::string const& d = sec->data;
        if (m_page == 0) {
            cycleRow("Main window", 0, profGet(d, "stage", "4 ticks"), 8.f);
            tickInput("Game speed (TPS)", "tps-value", -190.f, 70.f, -24.f, profGet(d, "tps-value", "240"), true);
            cycleRow("Dual mode", 5, profGet(d, "dual-mode", "0") == "1" ? "On" : "Off", -56.f);
        } else if (m_page == 1) {
            static constexpr char const* NAMES[] = {"Cube", "Ball", "Robot", "Spider", "Ship", "UFO", "Swing", "Wave"};
            for (int i = 0; i < 8; i++) {
                int col = i / 4, row = i % 4;
                float y = 8.f - 32.f * row;
                tickInput(NAMES[i], MODE_KEYS[i], col ? 10.f : -190.f, col ? 100.f : -100.f, y, profGet(d, MODE_KEYS[i], "-1"), false);
            }
        } else {
            cycleRow("Wave", 1, profGet(d, "wave-mode", "Grid"), 8.f);
            cycleRow("Ship", 2, profGet(d, "ship-style", "Min Hold"), -24.f);
            cycleRow("UFO", 3, profGet(d, "ufo-style", "Min Hold"), -56.f);
            cycleRow("Swing", 4, profGet(d, "swing-style", "Min Hold"), -88.f);
        }
        m_updating = false;
    }

    void updateMoreLabels() {
        if (m_nameLabel) {
            std::string nm = m_names.empty() ? "(none saved)" : m_names[std::clamp(m_copySel, 0, static_cast<int>(m_names.size()) - 1)];
            m_nameLabel->setString(nm.c_str());
        }
        if (m_emptyLabel) {
            std::string e = std::string("Empty parts use: ") + EMPTY_MODE_NAMES[m_ls.emptyMode];
            m_emptyLabel->setString(e.c_str());
        }
    }

    // The "More" page: copy a saved profile in, choose what empty parts use,
    // and copy the sections to another level.
    void buildMore() {
        text("Copy settings from a saved profile", -190.f, 12.f, 0.33f, true, true);
        arrow(false, -60.f, -12.f, menu_selector(SectionsPopup::onNamePrev), 0, true);
        m_nameLabel = text("", 20.f, -12.f, 0.4f, false, true);
        arrow(true, 100.f, -12.f, menu_selector(SectionsPopup::onNameNext), 0, true);
        m_pageNodes.push_back(button("Load", 160.f, -12.f, menu_selector(SectionsPopup::onLoadProfile), 0.5f));

        m_emptyLabel = text("", -190.f, -46.f, 0.36f, true, true);
        m_pageNodes.push_back(button("Change", 150.f, -46.f, menu_selector(SectionsPopup::onEmptyMode), 0.5f));

        text("Copy these sections to another level", -190.f, -74.f, 0.33f, true, true);
        m_copyInput = input(100.f, "Level ID", "0123456789", 10, -120.f, -98.f, true);
        m_copyInput->setScale(0.75f);
        m_pageNodes.push_back(button("Copy sections", 0.f, -98.f, menu_selector(SectionsPopup::onCopy), 0.5f));
        text("Recommended: build on a copy of the level, then copy to the main level.", 0.f, -124.f, 0.25f, false, true);
        updateMoreLabels();
    }

    void defaultStatus() {
        if (!Mod::get()->getSettingValue<bool>("sections")) {
            m_status->setString("OFF: turn on in Buffer > Advanced");
            m_status->setColor({255, 120, 120});
            return;
        }
        m_status->setColor({255, 255, 255});
        m_status->setString("Add: type a %, last death, or current position");
    }

    void refresh() {
        m_updating = true;
        int n = static_cast<int>(m_ls.secs.size());
        if (m_idx >= n) m_idx = n - 1;
        if (m_idx < 0) m_idx = 0;
        if (m_levelID == 0) {
            m_idxLabel->setString("Open from inside a level");
            m_startInput->setString("");
            m_endInput->setString("");
        } else if (n == 0) {
            m_idxLabel->setString("No sections yet");
            m_startInput->setString("");
            m_endInput->setString("");
        } else {
            std::string s = "Section " + std::to_string(m_idx + 1) + " / " + std::to_string(n);
            m_idxLabel->setString(s.c_str());
            m_startInput->setString(fmtPct(m_ls.secs[m_idx].start));
            m_endInput->setString(fmtPct(m_ls.secs[m_idx].end));
        }
        updateMoreLabels();
        m_updating = false;
    }

public:
    static SectionsPopup* create(int levelID) {
        auto ret = new SectionsPopup();
        if (ret->init(levelID)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void onPrev(CCObject*) {
        int n = static_cast<int>(m_ls.secs.size());
        if (n == 0) return;
        m_idx = (m_idx - 1 + n) % n;
        refresh();
        defaultStatus();
        buildPage();
    }

    void onNext(CCObject*) {
        int n = static_cast<int>(m_ls.secs.size());
        if (n == 0) return;
        m_idx = (m_idx + 1) % n;
        refresh();
        defaultStatus();
        buildPage();
    }

    void onPageWindow(CCObject*) { m_page = 0; defaultStatus(); buildPage(); }
    void onPageModes(CCObject*) {
        m_page = 1;
        m_status->setString("-1 = main window, 0 = off");
        buildPage();
    }
    void onPageMore(CCObject*) { m_page = 3; defaultStatus(); buildPage(); }
    void onPageStyles(CCObject*) { m_page = 2; defaultStatus(); buildPage(); }

    // Arrow on a cycling row. Tag = row * 2 + (1 for next, 0 for previous).
    void onCycle(CCObject* sender) {
        auto sec = current();
        if (!sec) return;
        int tag = static_cast<CCNode*>(sender)->getTag();
        int row = tag / 2;
        int dir = (tag % 2) ? 1 : -1;
        std::string& d = sec->data;
        if (row == 0) {
            int cur = indexOf(STAGE_NAMES, STAGE_COUNT, profGet(d, "stage", "4 ticks"), 1);
            d = profSet(d, "stage", STAGE_NAMES[(cur + dir + STAGE_COUNT) % STAGE_COUNT]);
        } else if (row == 5) {
            d = profSet(d, "dual-mode", profGet(d, "dual-mode", "0") == "1" ? "0" : "1");
        } else {
            std::string key = row == 1 ? "wave-mode" : FLY_STYLE_KEYS[row - 2];
            int cur = indexOf(WAVE_NAMES, WAVE_MODE_COUNT, profGet(d, key, row == 1 ? "Grid" : "Min Hold"), 0);
            d = profSet(d, key, WAVE_NAMES[(cur + dir + WAVE_MODE_COUNT) % WAVE_MODE_COUNT]);
        }
        commit();
        buildPage();
    }

    void onNamePrev(CCObject*) {
        if (m_names.empty()) return;
        int n = static_cast<int>(m_names.size());
        m_copySel = (m_copySel - 1 + n) % n;
        refresh();
    }

    void onNameNext(CCObject*) {
        if (m_names.empty()) return;
        int n = static_cast<int>(m_names.size());
        m_copySel = (m_copySel + 1) % n;
        refresh();
    }

    // Copies a saved profile's settings into the current section.
    void onLoadProfile(CCObject*) {
        auto sec = current();
        if (!sec) { m_status->setString("Add a section first."); return; }
        if (m_names.empty()) { m_status->setString("No saved profiles yet."); return; }
        std::string name = m_names[std::clamp(m_copySel, 0, static_cast<int>(m_names.size()) - 1)];
        std::string data = Mod::get()->getSavedValue<std::string>(profileDataKey(name), "");
        if (data.empty()) { m_status->setString("Could not load that profile."); return; }
        for (auto const& item : splitStr(data, '|')) {
            size_t eq = item.find('=');
            if (eq == std::string::npos) continue;
            sec->data = profSet(sec->data, item.substr(0, eq), item.substr(eq + 1));
        }
        commit();
        buildPage();
        m_status->setString(("Loaded '" + name + "'.").c_str());
    }

    // Add asks where the new section should start.
    void onAdd(CCObject*) {
        if (m_levelID == 0) { m_status->setString("Open this from inside a level."); return; }
        auto popup = AddSectionPopup::create([this](float pct) { this->addAt(pct); });
        if (popup) popup->show();
    }

    void addAt(float start) {
        start = roundPct(start);
        for (auto const& s : m_ls.secs) {
            if (std::fabs(s.start - start) < 0.005f) { m_status->setString("A section already starts there."); return; }
        }
        m_ls.secs.push_back(newSection(m_ls, start));
        sortSections(m_ls);
        for (size_t i = 0; i < m_ls.secs.size(); i++) {
            if (std::fabs(m_ls.secs[i].start - start) < 0.005f) m_idx = static_cast<int>(i);
        }
        commit();
        refresh();
        m_status->setString("Section added.");
        buildPage();
    }

    void onDelete(CCObject*) {
        if (!current()) return;
        m_ls.secs.erase(m_ls.secs.begin() + m_idx);
        commit();
        refresh();
        m_status->setString("Section deleted.");
        buildPage();
    }

    void onEmptyMode(CCObject*) {
        m_ls.emptyMode = (m_ls.emptyMode + 1) % EMPTY_MODE_COUNT;
        commit();
        refresh();
    }

    // Copies these sections (and the level's linked profile) to another level.
    void onCopy(CCObject*) {
        int target = std::atoi(m_copyInput->getString().c_str());
        if (target <= 0) { m_status->setString("Type the level ID to copy to."); return; }
        if (target == m_levelID) { m_status->setString("That is this level."); return; }
        if (m_ls.secs.empty()) { m_status->setString("There are no sections to copy."); return; }
        saveLevelSections(target, m_ls);
        std::string linked = levelProfileFor(m_levelID);
        if (!linked.empty()) setLevelProfile(target, linked);
        std::string msg = "Copied " + std::to_string(m_ls.secs.size()) + " sections to level " + std::to_string(target) + ".";
        m_status->setString(msg.c_str());
    }

    void onClose(CCObject* sender) {
        // Sections only take their final order once the window is closed.
        sortSections(m_ls);
        commit();
        Popup::onClose(sender);
    }
};

// Opens the Sections window. If sections are switched off, a message says so.
static void openSectionsWindow(int levelID) {
    if (auto p = SectionsPopup::create(levelID)) p->show();
    if (!Mod::get()->getSettingValue<bool>("sections")) {
        FLAlertLayer::create(
            "Sections are off",
            "Level sections are turned off, so nothing you set here will be used.\n\nTurn on <cy>Level sections</c> in <cy>Buffer > Advanced</c> to use them.",
            "OK"
        )->show();
    }
}

class ProfilesPopup : public geode::Popup {
protected:
    TextInput* m_nameInput = nullptr;
    CCLabelBMFont* m_selLabel = nullptr;
    CCLabelBMFont* m_levelLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;
    std::vector<std::string> m_names;
    int m_sel = -1;
    int m_levelID = 0;

    bool init(int levelID) {
        if (!Popup::init(360.f, 260.f)) return false;
        m_levelID = levelID;
        this->setTitle("Profiles");

        m_names = loadProfileNames();
        if (!m_names.empty()) {
            m_sel = 0;
            std::string linked = levelProfileFor(levelID);
            for (size_t i = 0; i < m_names.size(); i++) {
                if (m_names[i] == linked) m_sel = static_cast<int>(i);
            }
        }

        auto text = [&](char const* s, float x, float y, float scale) {
            auto label = CCLabelBMFont::create(s, "bigFont.fnt");
            label->setScale(scale);
            m_mainLayer->addChildAtPosition(label, Anchor::Center, {x, y});
            return label;
        };
        auto button = [&](char const* s, float x, float y, SEL_MenuHandler cb) {
            auto spr = ButtonSprite::create(s);
            spr->setScale(0.6f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
            m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        };

        button("Sections", 125.f, 100.f, menu_selector(ProfilesPopup::onSections));
        text("Saved profile", 0.f, 88.f, 0.45f);
        m_selLabel = text("", 0.f, 62.f, 0.6f);

        auto left = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        left->setScale(0.6f);
        auto leftBtn = CCMenuItemSpriteExtra::create(left, this, menu_selector(ProfilesPopup::onPrev));
        m_buttonMenu->addChildAtPosition(leftBtn, Anchor::Center, {-125.f, 62.f});

        auto right = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        right->setScale(0.6f);
        right->setFlipX(true);
        auto rightBtn = CCMenuItemSpriteExtra::create(right, this, menu_selector(ProfilesPopup::onNext));
        m_buttonMenu->addChildAtPosition(rightBtn, Anchor::Center, {125.f, 62.f});

        button("Load", -55.f, 30.f, menu_selector(ProfilesPopup::onLoad));
        button("Delete", 55.f, 30.f, menu_selector(ProfilesPopup::onDelete));

        text("Save current settings as:", 0.f, 2.f, 0.4f);
        m_nameInput = TextInput::create(190.f, "Profile name");
        m_nameInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
        m_nameInput->setMaxCharCount(20);
        m_mainLayer->addChildAtPosition(m_nameInput, Anchor::Center, {-45.f, -22.f});
        button("Save", 120.f, -22.f, menu_selector(ProfilesPopup::onSave));

        m_levelLabel = text("", 0.f, -58.f, 0.4f);
        button("Use for this level", -70.f, -86.f, menu_selector(ProfilesPopup::onLink));
        button("Clear", 85.f, -86.f, menu_selector(ProfilesPopup::onUnlink));

        m_status = text("", 0.f, -115.f, 0.4f);

        this->refresh();
        return true;
    }

    void refresh() {
        if (m_sel >= 0 && m_sel < static_cast<int>(m_names.size())) {
            m_selLabel->setString(m_names[m_sel].c_str());
        } else {
            m_selLabel->setString("(none saved)");
        }
        if (m_levelID == 0) {
            m_levelLabel->setString("Open this from inside a level to link a profile");
        } else {
            std::string n = levelProfileFor(m_levelID);
            std::string s = "This level uses: " + (n.empty() ? std::string("your current settings") : n);
            m_levelLabel->setString(s.c_str());
        }
    }

    void setStatus(std::string const& s) { m_status->setString(s.c_str()); }

    std::string selectedName() const {
        if (m_sel < 0 || m_sel >= static_cast<int>(m_names.size())) return "";
        return m_names[m_sel];
    }

public:
    static ProfilesPopup* create(int levelID) {
        auto ret = new ProfilesPopup();
        if (ret->init(levelID)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void onPrev(CCObject*) {
        if (m_names.empty()) return;
        int n = static_cast<int>(m_names.size());
        m_sel = (m_sel - 1 + n) % n;
        refresh();
    }

    void onNext(CCObject*) {
        if (m_names.empty()) return;
        int n = static_cast<int>(m_names.size());
        m_sel = (m_sel + 1) % n;
        refresh();
    }

    void onLoad(CCObject*) {
        std::string name = selectedName();
        if (name.empty()) { setStatus("No profile selected."); return; }
        if (applyProfileByName(name)) setStatus("Loaded '" + name + "'.");
        else setStatus("Could not load that profile.");
    }

    void onDelete(CCObject*) {
        std::string name = selectedName();
        if (name.empty()) { setStatus("No profile selected."); return; }
        Mod::get()->setSavedValue<std::string>(profileDataKey(name), "");
        m_names.erase(m_names.begin() + m_sel);
        saveProfileNames(m_names);
        forgetProfileEverywhere(name);
        if (m_sel >= static_cast<int>(m_names.size())) m_sel = static_cast<int>(m_names.size()) - 1;
        setStatus("Deleted '" + name + "'.");
        refresh();
    }

    void onSave(CCObject*) {
        std::string name = m_nameInput->getString();
        // trim spaces on both ends
        size_t a = name.find_first_not_of(' ');
        size_t b = name.find_last_not_of(' ');
        name = a == std::string::npos ? std::string() : name.substr(a, b - a + 1);
        if (name.empty()) { setStatus("Type a name first."); return; }

        bool exists = std::find(m_names.begin(), m_names.end(), name) != m_names.end();
        if (!exists) {
            m_names.push_back(name);
            saveProfileNames(m_names);
        }
        Mod::get()->setSavedValue<std::string>(profileDataKey(name), serializeProfile());
        for (size_t i = 0; i < m_names.size(); i++) {
            if (m_names[i] == name) m_sel = static_cast<int>(i);
        }
        m_nameInput->setString("");
        setStatus((exists ? "Updated '" : "Saved '") + name + "'.");
        refresh();
    }

    void onLink(CCObject*) {
        if (m_levelID == 0) { setStatus("Open this from inside a level."); return; }
        std::string name = selectedName();
        if (name.empty()) { setStatus("Pick a profile first."); return; }
        setLevelProfile(m_levelID, name);
        setStatus("This level will use '" + name + "'.");
        refresh();
    }

    void onSections(CCObject*) {
        openSectionsWindow(m_levelID);
    }

    void onUnlink(CCObject*) {
        if (m_levelID == 0) { setStatus("Open this from inside a level."); return; }
        setLevelProfile(m_levelID, "");
        setStatus("Level profile cleared.");
        refresh();
    }
};


// ---- main popup: Basic and Advanced tabs --------------------------------
static constexpr int PRESET_COUNT = 5;
static constexpr char const* PRESET_NAMES[PRESET_COUNT] = {"Off", "Strict", "Standard", "Relaxed", "Very relaxed"};
static constexpr char const* PRESET_STAGE[PRESET_COUNT] = {"Off", "2 ticks", "4 ticks", "8 ticks", "16 ticks"};
static constexpr char const* PRESET_INFO[PRESET_COUNT] = {
    "No buffering or timing help.",
    "Remembers a click about half a frame early.",
    "Remembers a click about 1 frame early. A good start.",
    "Remembers a click about 2 frames early.",
    "Remembers a click about 4 frames early. Very forgiving."
};
static constexpr char const* DEFAULT_PROFILE =
    "stage=4 ticks|wave-mode=Grid|tps-value=240|cube-ticks=-1|ball-ticks=-1|robot-ticks=-1|spider-ticks=-1|"
    "ship-ticks=-1|ufo-ticks=-1|swing-ticks=-1|wave-ticks=-1|dual-mode=0|"
    "ship-style=Min Hold|ufo-style=Min Hold|swing-style=Min Hold";

// Which quick setup matches the current settings, or -1 for custom.
static int currentPreset() {
    auto m = Mod::get();
    for (auto key : MODE_KEYS) {
        if (m->getSettingValue<int64_t>(key) != -1) return -1;
    }
    std::string stage = m->getSettingValue<std::string>("stage");
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (stage == PRESET_STAGE[i]) return i;
    }
    return -1;
}

class BufferPopup : public geode::Popup {
protected:
    bool m_advanced = false;
    int m_levelID = 0;
    std::vector<CCNode*> m_tabNodes;
    CCLabelBMFont* m_presetLabel = nullptr;
    CCLabelBMFont* m_presetInfo = nullptr;
    CCLabelBMFont* m_waveLabel = nullptr;
    CCLabelBMFont* m_dualLabel = nullptr;
    CCLabelBMFont* m_secLabel = nullptr;
    CCLabelBMFont* m_note = nullptr;

    bool init(int levelID) {
        if (!Popup::init(360.f, 260.f)) return false;
        m_levelID = levelID;
        this->setTitle("Input Buffer");
        tabButton("Basic", -60.f, menu_selector(BufferPopup::onTabBasic));
        tabButton("Advanced", 60.f, menu_selector(BufferPopup::onTabAdvanced));
        m_note = CCLabelBMFont::create("", "bigFont.fnt");
        m_note->setScale(0.35f);
        m_mainLayer->addChildAtPosition(m_note, Anchor::Center, {0.f, -122.f});
        this->build();
        return true;
    }

    void tabButton(char const* s, float x, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create(s);
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, 76.f});
    }

    CCLabelBMFont* text(char const* s, float x, float y, float scale) {
        auto label = CCLabelBMFont::create(s, "bigFont.fnt");
        label->setScale(scale);
        m_mainLayer->addChildAtPosition(label, Anchor::Center, {x, y});
        m_tabNodes.push_back(label);
        return label;
    }

    void button(char const* s, float x, float y, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create(s);
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        m_tabNodes.push_back(btn);
    }

    void arrow(bool flip, float x, float y, SEL_MenuHandler cb) {
        auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        spr->setScale(0.6f);
        spr->setFlipX(flip);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {x, y});
        m_tabNodes.push_back(btn);
    }

    // Rebuilds the content of the current tab.
    void build() {
        for (auto n : m_tabNodes) n->removeFromParent();
        m_tabNodes.clear();
        m_presetLabel = m_presetInfo = m_waveLabel = m_dualLabel = m_secLabel = nullptr;
        m_note->setString("");

        if (!m_advanced) {
            text("Quick setup", 0.f, 48.f, 0.45f);
            m_presetLabel = text("", 0.f, 26.f, 0.6f);
            arrow(false, -125.f, 26.f, menu_selector(BufferPopup::onPresetPrev));
            arrow(true, 125.f, 26.f, menu_selector(BufferPopup::onPresetNext));
            m_presetInfo = text("", 0.f, 4.f, 0.35f);

            text("Game speed (TPS)", 0.f, -24.f, 0.45f);
            auto input = TextInput::create(100.f, "240");
            input->setFilter("0123456789");
            input->setMaxCharCount(5);
            input->setString(std::to_string(g_tps));
            input->setCallback([](std::string const& s) {
                if (s.empty()) return;
                int v = std::clamp(std::atoi(s.c_str()), TPS_MIN, TPS_MAX);
                Mod::get()->setSettingValue<int64_t>("tps-value", static_cast<int64_t>(v));
            });
            m_mainLayer->addChildAtPosition(input, Anchor::Center, {0.f, -48.f});
            m_tabNodes.push_back(input);

            text("Not sure? Leave it on Standard and set your TPS.", 0.f, -80.f, 0.33f);
        } else {
            m_waveLabel = text("", -50.f, 46.f, 0.38f);
            button("Change", 125.f, 46.f, menu_selector(BufferPopup::onWaveStyle));
            m_dualLabel = text("", -50.f, 20.f, 0.38f);
            button("Change", 125.f, 20.f, menu_selector(BufferPopup::onToggleDual));
            m_secLabel = text("", -50.f, -6.f, 0.38f);
            button("Change", 125.f, -6.f, menu_selector(BufferPopup::onToggleSections));

            button("All settings", -75.f, -38.f, menu_selector(BufferPopup::onAllSettings));
            button("Profiles", 75.f, -38.f, menu_selector(BufferPopup::onProfiles));
            button("Sections", -75.f, -68.f, menu_selector(BufferPopup::onSections));
            button("Reset all", 75.f, -68.f, menu_selector(BufferPopup::onResetAll));
            text("Per-mode windows are in All settings.", 0.f, -96.f, 0.33f);
        }
        this->refresh();
    }

    void refresh() {
        auto m = Mod::get();
        if (m_presetLabel) {
            int p = currentPreset();
            m_presetLabel->setString(p >= 0 ? PRESET_NAMES[p] : "Custom");
            m_presetInfo->setString(p >= 0 ? PRESET_INFO[p] : "You changed the settings by hand.");
        }
        if (m_waveLabel) {
            std::string a = std::string("Wave style: ") + waveModeName();
            m_waveLabel->setString(a.c_str());
            std::string b = std::string("Dual mode (experimental): ") + (g_dualEnabled ? "On" : "Off");
            m_dualLabel->setString(b.c_str());
            std::string c = std::string("Level sections (experimental): ") + (m->getSettingValue<bool>("sections") ? "On" : "Off");
            m_secLabel->setString(c.c_str());
        }
    }

public:
    static BufferPopup* create(int levelID) {
        auto ret = new BufferPopup();
        if (ret->init(levelID)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void onTabBasic(CCObject*) { m_advanced = false; build(); }
    void onTabAdvanced(CCObject*) { m_advanced = true; build(); }

    // Picking a quick setup resets the per-mode windows to follow it.
    void setPreset(int p) {
        auto m = Mod::get();
        m->setSettingValue<std::string>("stage", PRESET_STAGE[p]);
        for (auto key : MODE_KEYS) m->setSettingValue<int64_t>(key, -1);
        refresh();
    }

    void onPresetPrev(CCObject*) {
        int p = currentPreset();
        setPreset(p < 0 ? 2 : (p + PRESET_COUNT - 1) % PRESET_COUNT);
    }

    void onPresetNext(CCObject*) {
        int p = currentPreset();
        setPreset(p < 0 ? 2 : (p + 1) % PRESET_COUNT);
    }

    void onWaveStyle(CCObject*) {
        int next = (g_waveMode + 1) % WAVE_MODE_COUNT;
        Mod::get()->setSettingValue<std::string>("wave-mode", WAVE_NAMES[next]);
        refresh();
    }

    void onToggleDual(CCObject*) {
        Mod::get()->setSettingValue<bool>("dual-mode", !g_dualEnabled);
        refresh();
    }

    void onToggleSections(CCObject*) {
        auto m = Mod::get();
        m->setSettingValue<bool>("sections", !m->getSettingValue<bool>("sections"));
        refresh();
    }

    void onAllSettings(CCObject*) { openSettingsPopup(Mod::get(), false); }
    void onProfiles(CCObject*) { if (auto p = ProfilesPopup::create(m_levelID)) p->show(); }
    void onSections(CCObject*) { openSectionsWindow(m_levelID); }

    void onResetAll(CCObject*) {
        applyProfileData(DEFAULT_PROFILE);
        m_note->setString("Settings reset to defaults.");
        this->build();
        m_note->setString("Settings reset to defaults.");
    }
};

$on_mod(Loaded) {
    loadSettings();
    restoreAfterLevel(); // undo a level profile left over from a crash
    listenForSettingChanges<std::string>("stage", [](std::string) { loadSettings(); });
    listenForSettingChanges<int64_t>("tps-value", [](int64_t) { loadSettings(); });
    listenForSettingChanges<std::string>("wave-mode", [](std::string) { loadSettings(); });
    for (auto key : MODE_KEYS) {
        listenForSettingChanges<int64_t>(key, [](int64_t) { loadSettings(); });
    }
    listenForSettingChanges<bool>("debug", [](bool) { loadSettings(); });
    listenForSettingChanges<bool>("dual-mode", [](bool) { loadSettings(); });
    for (auto key : FLY_STYLE_KEYS) {
        listenForSettingChanges<std::string>(key, [](std::string) { loadSettings(); });
    }

    log::info("[InputBuffer] BufferMod loaded (v22: profiles + keybinds). Main: {}, TPS: {}, wave: {}, dual: {}",
              STAGE_NAMES[g_stage], g_tps, waveModeName(), g_dualEnabled ? "ON" : "OFF");
}

$on_game(Loaded) {
    for (int i = 0; i < KEYBIND_COUNT; i++) {
        listenForKeybindSettingPresses(KEYBIND_SETTINGS[i], [i](Keybind const&, bool down, bool repeat, double) {
            hotkeyPressed(i, down, repeat);
        });
    }
}

class $modify(BufferPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();

        auto spr = ButtonSprite::create("Buffer");
        spr->setScale(0.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BufferPause::onBufferSettings));
        menu->addChild(btn);

        auto spr2 = ButtonSprite::create("Profiles");
        spr2->setScale(0.7f);
        auto btn2 = CCMenuItemSpriteExtra::create(spr2, this, menu_selector(BufferPause::onBufferProfiles));
        btn2->setPosition({0.f, 30.f});
        menu->addChild(btn2);

        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width - 60.f, 30.f});
        this->addChild(menu);
    }

    void onBufferSettings(CCObject*) {
        if (auto popup = BufferPopup::create(currentLevelID())) popup->show();
    }

    void onBufferProfiles(CCObject*) {
        if (auto popup = ProfilesPopup::create(currentLevelID())) popup->show();
    }
};

// Raises the reset flag on death, restart and checkpoint loads. The buffer
// layer clears its state on the next step.
class $modify(BufferReset, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        g_resetEpoch++;
    }

    void resetLevelFromStart() {
        PlayLayer::resetLevelFromStart();
        g_resetEpoch++;
    }

    void loadFromCheckpoint(CheckpointObject* object) {
        PlayLayer::loadFromCheckpoint(object);
        g_resetEpoch++;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        g_resetEpoch++;
        g_lastDeathPct = this->getCurrentPercent();
    }

    // Switches settings when the player moves into a different section.
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_secActive) return;
        if (m_level && m_level->isPlatformer()) return;
        std::string tok = sectionToken(this->getCurrentPercent());
        if (tok == g_secToken) return;
        g_secToken = tok;
        applyToken(tok, levelKeyFor(m_level));
        g_resetEpoch++; // start the new section with clean input state
        DBG("[InputBuffer] Section -> {}", tok);
    }

    // Leaving the level puts the player's own settings back.
    void onExit() {
        PlayLayer::onExit();
        g_secActive = false;
        g_secToken.clear();
        restoreAfterLevel();
    }

    // Applies the profile linked to this level, if there is one.
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        if (!m_level) return;
        int id = levelKeyFor(m_level);
        if (id == 0) return;
        std::string name = levelProfileFor(id);
        g_secActive = false;
        g_secToken.clear();
        LevelSections ls = loadLevelSections(id);
        bool useSections = Mod::get()->getSettingValue<bool>("sections") && !ls.secs.empty();
        if (name.empty() && !useSections) return;
        // Remember the player's own settings so they can be restored on exit.
        ensureSnapshot();
        if (!name.empty() && applyProfileByName(name)) {
            log::info("[InputBuffer] Applied profile '{}' for level {}", name, id);
            std::string msg = "Input Buffer profile: " + name;
            Notification::create(msg.c_str(), NotificationIcon::Success, 1.5f)->show();
        }
        if (useSections) {
            sortSections(ls);
            g_sec = ls;
            g_secActive = true;
            log::info("[InputBuffer] {} sections active for level {}", ls.secs.size(), id);
        }
    }
};

struct PState {
    bool held = false;
    bool pending = false;
    int pendingStart = 0;
    int lastRingCount = 0;
    bool releaseNext = false;

    int pressStep = -1;
    bool delayedRelease = false;
    int releaseAt = 0;

    std::vector<std::pair<bool, int>> waveQueue;
    int lastReleaseStep = -1000000;
};

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        int step = 0;
        int epoch = 0;
        int graceUntil = 0;
        PState ps[2];
    };

    // True in a one-player level with a dual portal: a single click is sent
    // as player 1 and the game applies it to both icons.
    bool sharedDualInput() {
        return m_gameState.m_isDualMode && m_levelSettings && !m_levelSettings->m_twoPlayerMode;
    }

    // Press or release for one icon. In a dual section each icon has its own
    // state and decides for itself, so nothing is mirrored here.
    void sendInput(PlayerObject* p, bool down) {
        if (down) p->pushButton(PlayerButton::Jump);
        else p->releaseButton(PlayerButton::Jump);
    }

    // When experimental dual support is off, nothing may stay held back from
    // an earlier section: deliver what is queued and drop pending rescues.
    void bypassCleanup() {
        auto f = m_fields.operator->();
        PlayerObject* players[2] = {m_player1, m_player2};
        for (int i = 0; i < 2; i++) {
            auto p = players[i];
            auto& s = f->ps[i];
            if (!p) { s = PState(); continue; }
            if (!s.waveQueue.empty()) flushWaveQueue(p, s, i);
            if (s.delayedRelease) {
                s.delayedRelease = false;
                s.held = false;
                sendInput(p, false);
            }
            if (s.releaseNext) {
                s.releaseNext = false;
                if (!s.held) sendInput(p, false);
            }
            s.pending = false;
        }
    }

    // Clears our state after a death / restart / checkpoint load.
    void syncState() {
        auto f = m_fields.operator->();
        if (f->epoch == g_resetEpoch) return;
        f->epoch = g_resetEpoch;

        bool inconsistent = false;
        for (auto& s : f->ps) {
            if (s.delayedRelease || s.releaseNext || !s.waveQueue.empty()) {
                inconsistent = true;
            }
            s = PState();
        }

        // Wave Grid / Min Gap are skipped briefly after a spawn so the first
        // clicks go straight through.
        f->graceUntil = f->step + spawnGraceSteps();

        // We may have held back a release the game never saw. Releasing a
        // button that is not held is harmless, so make sure nothing is stuck.
        if (inconsistent) {
            if (m_player1) m_player1->releaseButton(PlayerButton::Jump);
            if (m_player2) m_player2->releaseButton(PlayerButton::Jump);
        }
        DBG("[InputBuffer] state reset (death/restart), spawn grace until step {}{}",
            f->graceUntil, inconsistent ? ", released stuck button" : "");
    }

    void flushWaveQueue(PlayerObject* p, PState& s, int idx) {
        for (auto const& ev : s.waveQueue) {
            sendInput(p, ev.first);
        }
        s.waveQueue.clear();
    }

    void stepPlayer(PlayerObject* p, PState& s, int idx) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";

        if (!s.waveQueue.empty()) {
            if (!isFlyMode(p)) {
                flushWaveQueue(p, s, idx);
            } else {
                size_t applied = 0;
                while (applied < s.waveQueue.size() &&
                       step >= s.waveQueue[applied].second) {
                    auto ev = s.waveQueue[applied];
                    sendInput(p, ev.first);
                    DBG("[InputBuffer] {} {} {} {} applied at step {}",
                        tag, modeName(p), WAVE_NAMES[styleFor(p)], ev.first ? "PRESS" : "RELEASE", step);
                    applied++;
                }
                s.waveQueue.erase(s.waveQueue.begin(), s.waveQueue.begin() + applied);
            }
        }

        if (s.delayedRelease) {
            if (!isFlyMode(p) || step >= s.releaseAt) {
                s.delayedRelease = false;
                s.held = false;
                sendInput(p, false);
                DBG("[InputBuffer] {} MIN-HOLD release applied at step {} [{}]",
                    tag, step, modeName(p));
            }
        }

        if (s.releaseNext) {
            s.releaseNext = false;
            if (!s.held) sendInput(p, false);
        }

        if (!s.pending) return;

        int waited = step - s.pendingStart;
        int limit = modeLimitSteps(p);

        if (limit <= 0 || waited > limit || !isGroundMode(p)) {
            DBG("[InputBuffer] {} pending EXPIRED waited={} limit={} mode={}",
                tag, waited, limit, modeName(p));
            s.pending = false;
            return;
        }

        int rings = ringsTouching(p);
        if (rings != s.lastRingCount) {
            DBG("[InputBuffer] {} ring overlap count {} -> {} (waited {})",
                tag, s.lastRingCount, rings, waited);
            s.lastRingCount = rings;
        }

        bool onGround = p->m_isOnGround;
        bool onRing = rings > 0;

        if (onGround || onRing) {
            s.pending = false;
            float ticksSaved = waited / stepsPerTick();
            char const* what = onRing ? "RING" : "GROUND";

            if (s.held) {
                DBG("[InputBuffer] {} reached {} after {} steps but button still held, skipping ({})",
                    tag, what, waited, modeName(p));
            } else {
                sendInput(p, true);
                s.releaseNext = true;
                log::info("[InputBuffer] *** SAVED +{:.1f} TICKS ({:.2f} frames @ 60fps) PRESS! [{} {} {}] (Step0: {} -> Delayed: {}) [Limit: {} steps] ***",
                          ticksSaved, ticksSaved / 4.0f, tag, modeName(p), what,
                          s.pendingStart, step, limit);
            }
        }
    }

    bool inputPlayer(PlayerObject* p, PState& s, int idx, bool down) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";

        if (!isFlyMode(p) && !s.waveQueue.empty()) {
            flushWaveQueue(p, s, idx);
        }

        int n = modeLimitSteps(p);
        int style = styleFor(p);
        bool inGrace = step < m_fields->graceUntil;
        if (isFlyMode(p) && inGrace && n > 0 && style != WAVE_HOLD) {
            DBG("[InputBuffer] {} {} {} {} passed through (spawn grace) at step {}",
                tag, modeName(p), WAVE_NAMES[style], down ? "PRESS" : "RELEASE", step);
        }

        if (isFlyMode(p) && n > 0 && style != WAVE_HOLD && !inGrace) {
            int target;
            if (style == WAVE_GRID) {
                target = ((step + n - 1) / n) * n;
                if (!s.waveQueue.empty()) {
                    auto const& last = s.waveQueue.back();
                    target = std::max(target, last.second);
                    // A tap must hold for at least one grid cell, or the
                    // press and release would cancel out in the same step.
                    if (!down && last.first) {
                        target = std::max(target, last.second + n);
                    }
                }
            } else { // WAVE_GAP
                target = step;
                if (!s.waveQueue.empty()) {
                    target = std::max(target, s.waveQueue.back().second);
                }
                if (down) {
                    target = std::max(target, s.lastReleaseStep + n);
                } else {
                    s.lastReleaseStep = target;
                }
                if (target == step && s.waveQueue.empty()) {
                    s.held = down;
                    return false;
                }
            }

            s.held = down;
            s.waveQueue.emplace_back(down, target);
            DBG("[InputBuffer] {} {} {} {} queued at step {} -> {} ({} steps)",
                tag, modeName(p), WAVE_NAMES[style], down ? "PRESS" : "RELEASE", step, target, n);
            return true;
        }

        if (down && s.delayedRelease) {
            s.delayedRelease = false;
            s.held = false;
            sendInput(p, false);
        }

        if (!down && isFlyMode(p) && s.pressStep >= 0) {
            int minHold = modeLimitSteps(p);
            int heldFor = step - s.pressStep;
            if (minHold > 0 && heldFor < minHold) {
                s.delayedRelease = true;
                s.releaseAt = s.pressStep + minHold;
                DBG("[InputBuffer] {} MIN-HOLD delaying release: held {} < {} steps [{}]",
                    tag, heldFor, minHold, modeName(p));
                return true;
            }
        }

        s.held = down;

        if (down) {
            s.pressStep = isFlyMode(p) ? step : -1;

            bool early = isGroundMode(p) && !p->m_isOnGround && ringsTouching(p) == 0;
            if (early && modeLimitSteps(p) > 0) {
                s.pending = true;
                s.pendingStart = step;
                s.lastRingCount = 0;
            } else {
                s.pending = false;
            }
        }
        return false;
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto f = m_fields.operator->();
        f->step++;
        syncState();

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (!PlayLayer::get()) {
            for (auto& s : f->ps) s = PState();
            return;
        }

        // Experimental dual support is off: plain game input in dual sections.
        if (sharedDualInput() && !g_dualEnabled) {
            bypassCleanup();
            return;
        }

        if (m_player1) stepPlayer(m_player1, f->ps[0], 0);
        if (m_player2) stepPlayer(m_player2, f->ps[1], 1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto f = m_fields.operator->();
        syncState();

        if (PlayLayer::get() && button == 1) {
            // Outside two-player mode the game applies every input to player 1,
            // even one that arrives flagged as player 2 (for example a Jump P2
            // keybind). Treat those inputs as player 1 so the right icon is helped.
            bool asPlayer1 = isPlayer1 || !(m_levelSettings && m_levelSettings->m_twoPlayerMode);

            // Dual portal in a one-player level: the game applies a single
            // click to both icons.
            if (asPlayer1 && m_player1 && sharedDualInput()) {
                if (!g_dualEnabled) {
                    // Experimental dual support is off: plain game input.
                    bypassCleanup();
                    f->ps[0].held = down;
                    f->ps[1].held = down;
                    GJBaseGameLayer::handleButton(down, button, isPlayer1);
                    return;
                }

                // Each icon follows its own game mode and its own state.
                bool hold1 = inputPlayer(m_player1, f->ps[0], 0, down);
                bool hold2 = m_player2 ? inputPlayer(m_player2, f->ps[1], 1, down) : false;
                if (hold1 && hold2) return;
                if (!hold1 && !hold2) {
                    GJBaseGameLayer::handleButton(down, button, isPlayer1);
                    return;
                }
                // Only one icon is held back: give the click to the other now.
                auto now = hold1 ? m_player2 : m_player1;
                if (now) sendInput(now, down);
                return;
            }

            int idx = asPlayer1 ? 0 : 1;
            auto p = asPlayer1 ? m_player1 : m_player2;
            if (p) {
                if (inputPlayer(p, f->ps[idx], idx, down)) return;
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};