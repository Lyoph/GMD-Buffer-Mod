#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// ---- settings --------------------------------------------------------
static constexpr char const* STAGE_NAMES[] = {"2 ticks", "4 ticks", "8 ticks", "16 ticks", "Custom", "Off"};
static constexpr double STAGES[] = {2, 4, 8, 16, -1, 0};
static constexpr int STAGE_COUNT = 6;
static int g_stage = 1;
static double g_stageCustom = 1.0;

static constexpr double WAVE_MAX_TICKS = 8;

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
static double g_modeTicks[MODE_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1};

static constexpr char const* WAVE_NAMES[] = {"Min Hold", "Min Gap", "Grid"};
enum WaveMode { WAVE_HOLD = 0, WAVE_GAP = 1, WAVE_GRID = 2 };
static constexpr int WAVE_MODE_COUNT = 3;
static int g_waveMode = WAVE_GRID;

static constexpr char const* TPS_NAMES[] = {"60", "120", "240", "360", "480", "720", "960", "1000"};
static constexpr int TPS_PRESETS[] = {60, 120, 240, 360, 480, 720, 960, 1000};
static constexpr int TPS_COUNT = 8;
static int g_tpsIndex = 2;

static bool g_debug = false;
static bool g_cbfSplit = true;
static int g_splitSlices = 8;

// Hotkeys. Order: 0 stage, 1 tps up, 2 tps down, 3 debug, 4 wave mode.
static constexpr int KEY_COUNT = 5;
static constexpr char const* KEY_SETTINGS[] = {"key-stage", "key-tps-up", "key-tps-down", "key-debug", "key-wave"};
static int g_keyVK[KEY_COUNT] = {0, 0, 0, 0, 0}; // 0 = none

// Raised whenever the level resets (death, restart, checkpoint).
static int g_resetEpoch = 0;
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
        g_modeTicks[i] = m->getSettingValue<double>(MODE_KEYS[i]);
    }
    for (int i = 0; i < KEY_COUNT; i++) {
        g_keyVK[i] = vkFromName(m->getSettingValue<std::string>(KEY_SETTINGS[i]));
    }
    g_stageCustom = m->getSettingValue<double>("stage-custom");
    g_cbfSplit = m->getSettingValue<bool>("cbf-split");
    g_splitSlices = static_cast<int>(m->getSettingValue<int64_t>("split-slices"));
    g_debug = m->getSettingValue<bool>("debug");
}

static char const* waveModeName() { return WAVE_NAMES[g_waveMode]; }

static float stepsPerTick() {
    return TPS_PRESETS[g_tpsIndex] / 240.0f;
}

static double stageTicks() {
    double t = STAGES[g_stage];
    return t < 0 ? g_stageCustom : t;
}

static int ticksToSteps(double ticks) {
    if (ticks <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(ticks * stepsPerTick())));
}

static int mainLimitSteps() {
    return ticksToSteps(stageTicks());
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

static double ticksForMode(int idx) {
    double c = g_modeTicks[idx];
    if (c < 0) {
        double t = stageTicks();
        if (idx == MODE_WAVE && t > 0) t = std::min(t, WAVE_MAX_TICKS);
        return t;
    }
    return c;
}

static int modeLimitSteps(PlayerObject* p) {
    return ticksToSteps(ticksForMode(modeIndex(p)));
}

static int waveGridSteps() {
    return ticksToSteps(ticksForMode(MODE_WAVE));
}

static int spawnGraceSteps() {
    return ticksToSteps(SPAWN_GRACE_TICKS);
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
            log::info("[InputBuffer] Main window -> {} ({:.2f} ticks = {} steps, wave {} steps @ {} TPS)",
                      STAGE_NAMES[g_stage], stageTicks(), mainLimitSteps(), waveGridSteps(), TPS_PRESETS[g_tpsIndex]);
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
    listenForSettingChanges<double>("stage-custom", [](double) { loadSettings(); });
    listenForSettingChanges<std::string>("tps", [](std::string) { loadSettings(); });
    listenForSettingChanges<std::string>("wave-mode", [](std::string) { loadSettings(); });
    for (auto key : MODE_KEYS) {
        listenForSettingChanges<double>(key, [](double) { loadSettings(); });
    }
    for (auto key : KEY_SETTINGS) {
        listenForSettingChanges<std::string>(key, [](std::string) { loadSettings(); });
    }
    listenForSettingChanges<bool>("debug", [](bool) { loadSettings(); });
    listenForSettingChanges<bool>("cbf-split", [](bool) { loadSettings(); });
    listenForSettingChanges<int64_t>("split-slices", [](int64_t) { loadSettings(); });

    log::info("[InputBuffer] BufferMod loaded (v19: wave tap + spawn fix). Main: {}, TPS: {}, wave: {}",
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
struct SplitState {
    bool midStep = false;
    PlayerObject* player = nullptr;
    float rotationDelta = 0.f;
    CCPoint startPos{};
};
static SplitState g_split;

static void resetCollisionLog(PlayerObject* p) {
    if (p->m_collisionLogTop) p->m_collisionLogTop->removeAllObjects();
    if (p->m_collisionLogBottom) p->m_collisionLogBottom->removeAllObjects();
    if (p->m_collisionLogLeft) p->m_collisionLogLeft->removeAllObjects();
    if (p->m_collisionLogRight) p->m_collisionLogRight->removeAllObjects();
    p->m_lastCollisionLeft = -1;
    p->m_lastCollisionRight = -1;
    p->m_lastCollisionBottom = -1;
    p->m_lastCollisionTop = -1;
}

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

    // Press or release for the icon at idx. In shared dual input, player 1's
    // events also go to player 2 (rescued clicks only if player 2 is in a
    // landing mode, so a cube rescue does not tap a wave).
    void sendInput(PlayerObject* p, int idx, bool down, bool groundOnly) {
        auto one = [&](PlayerObject* q) {
            if (down) q->pushButton(PlayerButton::Jump);
            else q->releaseButton(PlayerButton::Jump);
        };
        one(p);
        if (idx == 0 && m_player2 && sharedDualInput()) {
            if (!groundOnly || isGroundMode(m_player2)) one(m_player2);
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
            sendInput(p, idx, ev.first, false);
        }
        s.waveQueue.clear();
    }

    void stepPlayer(PlayerObject* p, PState& s, int idx) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";

        if (!s.waveQueue.empty()) {
            if (!p->m_isDart) {
                flushWaveQueue(p, s, idx);
            } else {
                size_t applied = 0;
                while (applied < s.waveQueue.size() &&
                       step >= s.waveQueue[applied].second) {
                    auto ev = s.waveQueue[applied];
                    sendInput(p, idx, ev.first, false);
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
                sendInput(p, idx, false, false);
                DBG("[InputBuffer] {} MIN-HOLD release applied at step {} [{}]",
                    tag, step, modeName(p));
            }
        }

        if (s.releaseNext) {
            s.releaseNext = false;
            if (!s.held) sendInput(p, idx, false, true);
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

        if (onGround || onRing) firePending(p, s, idx, onRing, 0.0);
    }

    void firePending(PlayerObject* p, PState& s, int idx, bool onRing, double fraction) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";
        char const* what = onRing ? "RING" : "GROUND";
        double waited = step - s.pendingStart + fraction;
        s.pending = false;

        if (s.held) {
            DBG("[InputBuffer] {} reached {} after {:.3f} steps but button still held, skipping ({})",
                tag, what, waited, modeName(p));
            return;
        }
        sendInput(p, idx, true, true);
        s.releaseNext = true;
        double ticksSaved = waited / stepsPerTick();
        log::info("[InputBuffer] *** SAVED +{:.3f} TICKS ({:.2f} frames @ 60fps) PRESS! [{} {} {}] (Step0: {} -> Delayed: {} +{:.3f}) [Limit: {} steps] ***",
                  ticksSaved, ticksSaved / 4.0, tag, modeName(p), what,
                  s.pendingStart, step, fraction, modeLimitSteps(p));
    }

    bool splitLanding(PlayerObject* p, float dt) {
        if (!g_cbfSplit || g_split.midStep || dt <= 0.f || !PlayLayer::get()) return false;
        int idx = p == m_player1 ? 0 : p == m_player2 ? 1 : -1;
        if (idx < 0) return false;

        auto f = m_fields.operator->();
        auto& s = f->ps[idx];
        if (!s.pending || s.held || !isGroundMode(p)) return false;
        if (p->m_isOnGround || ringsTouching(p) > 0) return false;
        int limit = modeLimitSteps(p);
        if (limit <= 0 || f->step - s.pendingStart > limit) return false;

        int slices = std::clamp(g_splitSlices, 2, 64);
        float lead = dt / slices;
        g_split.midStep = true;
        g_split.player = p;
        g_split.startPos = p->getPosition();

        int done = 0;
        bool fired = false;
        while (done < slices - 1) {
            p->update(lead);
            done++;
            bool slope = p->m_isOnSlope && !p->m_isDart;
            this->checkCollisions(p, slope ? dt : 0.f, true);
            p->updateRotation(lead);
            resetCollisionLog(p);
            if (p->m_isDead) break;

            int rings = ringsTouching(p);
            if (p->m_isOnGround || rings > 0) {
                firePending(p, s, idx, rings > 0, static_cast<double>(done) / slices);
                fired = true;
                break;
            }
        }

        float rest = dt - lead * done;
        g_split.rotationDelta = rest;
        p->update(rest);
        g_split.midStep = false;
        if (!fired) DBG("[InputBuffer] split step {}: no contact in {} slices", f->step, slices);
        return true;
    }

    bool inputPlayer(PlayerObject* p, PState& s, int idx, bool down) {
        int step = m_fields->step;
        char const* tag = idx == 0 ? "P1" : "P2";

        if (!p->m_isDart && !s.waveQueue.empty()) {
            flushWaveQueue(p, s, idx);
        }

        int n = waveGridSteps();
        bool inGrace = step < m_fields->graceUntil;
        if (p->m_isDart && inGrace && n > 0 && g_waveMode != WAVE_HOLD) {
            DBG("[InputBuffer] {} WAVE {} {} passed through (spawn grace) at step {}",
                tag, waveModeName(), down ? "PRESS" : "RELEASE", step);
        }

        if (p->m_isDart && n > 0 && g_waveMode != WAVE_HOLD && !inGrace) {
            int target;
            if (g_waveMode == WAVE_GRID) {
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
            DBG("[InputBuffer] {} WAVE {} {} queued at step {} -> {} ({} steps)",
                tag, waveModeName(), down ? "PRESS" : "RELEASE", step, target, n);
            return true;
        }

        if (down && s.delayedRelease) {
            s.delayedRelease = false;
            s.held = false;
            sendInput(p, idx, false, false);
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
        syncState();

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (!PlayLayer::get()) {
            for (auto& s : f->ps) s = PState();
            return;
        }

        if (m_player1) stepPlayer(m_player1, f->ps[0], 0);
        if (m_player2) stepPlayer(m_player2, f->ps[1], 1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto f = m_fields.operator->();
        syncState();

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

class $modify(BufferPlayer, PlayerObject) {
    void update(float dt) {
        if (!g_split.midStep) {
            auto gl = GJBaseGameLayer::get();
            if (gl && static_cast<BufferLayer*>(gl)->splitLanding(this, dt)) return;
        }
        PlayerObject::update(dt);
    }
    void updateRotation(float t) {
        if (!g_split.midStep && g_split.player == this) {
            g_split.player = nullptr;
            PlayerObject::updateRotation(g_split.rotationDelta);
            m_lastPosition = g_split.startPos;
            return;
        }
        PlayerObject::updateRotation(t);
    }
};
