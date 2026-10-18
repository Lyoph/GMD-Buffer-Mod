#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// ---- settings --------------------------------------------------------
static constexpr char const* STAGE_NAMES[] = {"2 ticks", "4 ticks", "8 ticks", "16 ticks", "Off"};
static constexpr int STAGES[] = {2, 4, 8, 16, 0};
static constexpr int STAGE_COUNT = 5;
static int g_stage = 1;

static constexpr int WAVE_MAX_TICKS = 8;

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

static constexpr char const* TPS_NAMES[] = {"60", "120", "240", "360", "480", "720", "960", "1000"};
static constexpr int TPS_PRESETS[] = {60, 120, 240, 360, 480, 720, 960, 1000};
static constexpr int TPS_COUNT = 8;
static int g_tpsIndex = 2;

static bool g_debug = false;

// Hotkeys. Order: 0 stage, 1 tps up, 2 tps down, 3 debug, 4 wave mode.
static constexpr int KEY_COUNT = 5;
static constexpr char const* KEY_SETTINGS[] = {"key-stage", "key-tps-up", "key-tps-down", "key-debug", "key-wave"};
static int g_keyVK[KEY_COUNT] = {0, 0, 0, 0, 0}; // 0 = none
// ----------------------------------------------------------------------

#define DBG(...) do { if (g_debug) log::info(__VA_ARGS__); } while (0)

static int indexOf(char const* const* names, int n, std::string const& v, int def) {
    for (int i = 0; i < n; i++) if (v == names[i]) return i;
    return def;
}

static int vkFromName(std::string const& n) {
#ifdef GEODE_IS_WINDOWS
    if (n == "None") return 0;
    if (n.size() >= 2 && n[0] == 'F') {
        int num = std::atoi(n.c_str() + 1);
        if (num >= 1 && num <= 12) return VK_F1 + (num - 1);
    }
    if (n == "Insert") return VK_INSERT;
    if (n == "Delete") return VK_DELETE;
    if (n == "Home") return VK_HOME;
    if (n == "End") return VK_END;
    if (n == "Page Up") return VK_PRIOR;
    if (n == "Page Down") return VK_NEXT;
#endif
    return 0;
}

static void loadSettings() {
    auto m = Mod::get();
    g_stage = indexOf(STAGE_NAMES, STAGE_COUNT, m->getSettingValue<std::string>("stage"), 1);
    g_tpsIndex = indexOf(TPS_NAMES, TPS_COUNT, m->getSettingValue<std::string>("tps"), 2);
    g_waveMode = indexOf(WAVE_NAMES, WAVE_MODE_COUNT, m->getSettingValue<std::string>("wave-mode"), WAVE_GRID);
    for (int i = 0; i < MODE_COUNT; i++) {
        g_modeTicks[i] = static_cast<int>(m->getSettingValue<int64_t>(MODE_KEYS[i]));
    }
    for (int i = 0; i < KEY_COUNT; i++) {
        g_keyVK[i] = vkFromName(m->getSettingValue<std::string>(KEY_SETTINGS[i]));
    }
    g_debug = m->getSettingValue<bool>("debug");
}

static char const* waveModeName() { return WAVE_NAMES[g_waveMode]; }

static float stepsPerTick() {
    return TPS_PRESETS[g_tpsIndex] / 240.0f;
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

static void doHotkey(int action) {
    auto m = Mod::get();
    switch (action) {
        case 0:
            g_stage = (g_stage + 1) % STAGE_COUNT;
            m->setSettingValue<std::string>("stage", STAGE_NAMES[g_stage]);
            log::info("[InputBuffer] Main window -> {} ({} steps, wave {} steps @ {} TPS)",
                      STAGE_NAMES[g_stage], mainLimitSteps(), waveGridSteps(), TPS_PRESETS[g_tpsIndex]);
            break;
        case 1:
            g_tpsIndex = std::min(g_tpsIndex + 1, TPS_COUNT - 1);
            m->setSettingValue<std::string>("tps", TPS_NAMES[g_tpsIndex]);
            log::info("[InputBuffer] TPS -> {} ({:.2f} steps/tick)", TPS_PRESETS[g_tpsIndex], stepsPerTick());
            break;
        case 2:
            g_tpsIndex = std::max(g_tpsIndex - 1, 0);
            m->setSettingValue<std::string>("tps", TPS_NAMES[g_tpsIndex]);
            log::info("[InputBuffer] TPS -> {} ({:.2f} steps/tick)", TPS_PRESETS[g_tpsIndex], stepsPerTick());
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
    }
}

// Rebindable hotkeys (edge-detected polling).
static void pollKeys() {
#ifdef GEODE_IS_WINDOWS
    static bool prev[KEY_COUNT] = {false, false, false, false, false};
    bool now[KEY_COUNT];
    for (int i = 0; i < KEY_COUNT; i++) {
        now[i] = g_keyVK[i] != 0 && (GetAsyncKeyState(g_keyVK[i]) & 0x8000) != 0;
    }
    for (int i = 0; i < KEY_COUNT; i++) {
        if (now[i] && !prev[i]) doHotkey(i);
    }
    for (int i = 0; i < KEY_COUNT; i++) prev[i] = now[i];
#endif
}

$on_mod(Loaded) {
    loadSettings();
    listenForSettingChanges<std::string>("stage", [](std::string) { loadSettings(); });
    listenForSettingChanges<std::string>("tps", [](std::string) { loadSettings(); });
    listenForSettingChanges<std::string>("wave-mode", [](std::string) { loadSettings(); });
    for (auto key : MODE_KEYS) {
        listenForSettingChanges<int64_t>(key, [](int64_t) { loadSettings(); });
    }
    for (auto key : KEY_SETTINGS) {
        listenForSettingChanges<std::string>(key, [](std::string) { loadSettings(); });
    }
    listenForSettingChanges<bool>("debug", [](bool) { loadSettings(); });

    log::info("[InputBuffer] BufferMod loaded (v16: rebindable keys). Main: {}, TPS: {}, wave: {}",
              STAGE_NAMES[g_stage], TPS_PRESETS[g_tpsIndex], waveModeName());
}

class $modify(BufferPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();
        auto spr = ButtonSprite::create("Buffer");
        spr->setScale(0.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BufferPause::onBufferSettings));
        menu->addChild(btn);

        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width - 60.f, 30.f});
        this->addChild(menu);
    }

    void onBufferSettings(CCObject*) {
        openSettingsPopup(Mod::get(), false);
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
        PState ps[2];
    };

    static void applyWaveEvent(PlayerObject* p, bool down) {
        if (down) p->pushButton(PlayerButton::Jump);
        else p->releaseButton(PlayerButton::Jump);
    }

    static void flushWaveQueue(PlayerObject* p, PState& s) {
        for (auto const& ev : s.waveQueue) {
            applyWaveEvent(p, ev.first);
        }
        s.waveQueue.clear();
    }

    void stepPlayer(PlayerObject* p, PState& s, int idx) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";

        if (!s.waveQueue.empty()) {
            if (!p->m_isDart) {
                flushWaveQueue(p, s);
            } else {
                size_t applied = 0;
                while (applied < s.waveQueue.size() &&
                       step >= s.waveQueue[applied].second) {
                    auto ev = s.waveQueue[applied];
                    applyWaveEvent(p, ev.first);
                    DBG("[InputBuffer] {} WAVE {} {} applied at step {}",
                        tag, waveModeName(), ev.first ? "PRESS" : "RELEASE", step);
                    applied++;
                }
                s.waveQueue.erase(s.waveQueue.begin(), s.waveQueue.begin() + applied);
            }
        }

        if (s.delayedRelease) {
            if (!isFlyMode(p) || step >= s.releaseAt) {
                s.delayedRelease = false;
                s.held = false;
                p->releaseButton(PlayerButton::Jump);
                DBG("[InputBuffer] {} MIN-HOLD release applied at step {} [{}]",
                    tag, step, modeName(p));
            }
        }

        if (s.releaseNext) {
            s.releaseNext = false;
            if (!s.held) p->releaseButton(PlayerButton::Jump);
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
                p->pushButton(PlayerButton::Jump);
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

        if (!p->m_isDart && !s.waveQueue.empty()) {
            flushWaveQueue(p, s);
        }

        int n = waveGridSteps();
        if (p->m_isDart && n > 0 && g_waveMode != WAVE_HOLD) {
            int target;
            if (g_waveMode == WAVE_GRID) {
                target = ((step + n - 1) / n) * n;
                if (!s.waveQueue.empty()) {
                    target = std::max(target, s.waveQueue.back().second);
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
            DBG("[InputBuffer] {} WAVE {} {} queued at step {} -> {} ({} steps)",
                tag, waveModeName(), down ? "PRESS" : "RELEASE", step, target, n);
            return true;
        }

        if (down && s.delayedRelease) {
            s.delayedRelease = false;
            s.held = false;
            p->releaseButton(PlayerButton::Jump);
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
        pollKeys();

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (!PlayLayer::get()) {
            for (auto& s : f->ps) {
                s.delayedRelease = false;
                s.waveQueue.clear();
                s.pending = false;
            }
            return;
        }

        if (m_player1) stepPlayer(m_player1, f->ps[0], 0);
        if (m_player2) stepPlayer(m_player2, f->ps[1], 1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto f = m_fields.operator->();

        if (PlayLayer::get() && button == 1) {
            int idx = isPlayer1 ? 0 : 1;
            auto p = isPlayer1 ? m_player1 : m_player2;
            if (p) {
                if (inputPlayer(p, f->ps[idx], idx, down)) return;
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};