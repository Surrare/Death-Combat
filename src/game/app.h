// Application: scenes (title, character select, hunt, fight), presentation and HUD.
#pragma once
#include <string>
#include <vector>
#include "core/math.h"
#include "gfx/renderer.h"
#include "game/input.h"
#include "game/characters.h"
#include "game/fight.h"
#include "game/fight_ai.h"
#include "game/rig.h"
#include "game/hunt.h"
#include "game/settings.h"

struct GLFWwindow;

namespace dc {

struct AppOptions {
    std::string screenshotPath;   // if set: capture after screenshotTime seconds and quit
    float screenshotTime = 0;
    int demo = 0;                 // 0 none, 1 = AI vs AI fight, 2 = hunt, 3 = character select
    uint64_t seed = 0;
};

struct Particle {
    Vec3 pos, vel;
    float life = 0, maxLife = 1, size = 0.1f, growth = 0, gravity = 0;
    Color col;
    Blend blend = Blend::Additive;
};

struct FloatText {
    Vec3 pos;
    std::string text;
    Color col;
    float life = 0, maxLife = 1, scale = 2;
};

struct Banner {
    std::string text;
    Color col;
    float life = 0, maxLife = 1, scale = 5, yFrac = 0.4f;
};

enum class Scene { Title, Select, Hunt, Fight, Settings };
enum class FightMode { Hunt, VsCpu, Versus, Demo };

class App {
public:
    bool init(GLFWwindow* window, const AppOptions& opts);
    void frame(float dt, int width, int height);
    bool wantsQuit() const { return quit_; }

private:
    // scenes
    void updateTitle(float dt);
    void renderTitle();
    void updateSelect(float dt);
    void renderSelect();
    void updateHunt(float dt);
    void renderHunt();
    void updateSettings(float dt);
    void renderSettings();
    void openSettings(Scene returnTo);
    void closeSettings();
    void applyVideoSettings();
    void updateFight(float dt);
    void renderFight();
    void drawFightHud();
    void drawPauseMenu();
    void drawMoveList();

    void startHunt(int playerChar);
    void startFight(int charA, int charB, FightMode mode, int difficulty);
    void processEvents();
    void updateEffects(float dt);
    void spawnSparks(Vec3 pos, int n, Color a, Color b, float speed, float size, float life, Blend blend, float gravity);
    void addBanner(const std::string& t, Color c, float life, float scale, float yFrac = 0.4f);
    void addFloat(Vec3 pos, const std::string& t, Color c, float life, float scale);
    bool project(const Vec3& p, float& sx, float& sy) const;
    void drawPanelText(float x, float y, const std::vector<std::string>& lines, float scale, Color col, float lineGap = 1.5f);
    void buildArena();
    void drawCharacterCard(int id, float x, float y, float w, float h, bool highlighted, bool defeated);
    void takeScreenshot(const std::string& path);
    SimInput padToSim(const PadState& p) const;

    GLFWwindow* window_ = nullptr;
    AppOptions opts_;
    Renderer renderer_;
    RigRenderer rig_;
    Input input_;
    Settings settings_;

    // settings screen
    Scene settingsReturn_ = Scene::Title;
    int settingsIndex_ = 0, settingsSlot_ = 0;
    bool settingsCapturing_ = false;
    int windowedX_ = 100, windowedY_ = 100, windowedW_ = 1280, windowedH_ = 720;
    Scene scene_ = Scene::Title;
    float time_ = 0;
    bool quit_ = false;
    int width_ = 1280, height_ = 720;
    bool shotTaken_ = false;
    int shotCounter_ = 0;
    bool f12Was_ = false;

    // title
    int menuIndex_ = 0;
    float titleTime_ = 0;
    Mesh titleFloor_;

    // select
    int selectFor_ = 0;           // which player is choosing
    int selCursor_[2] = {0, 1};
    int selChosen_[2] = {-1, -1};
    FightMode pendingMode_ = FightMode::Hunt;
    float selectSpin_ = 0;

    // hunt
    Hunt hunt_;
    int huntPlayerChar_ = 0;
    std::vector<int> huntOrder_;
    int huntEngaged_ = -1;
    float huntEngageTimer_ = 0;
    float huntBannerTimer_ = 0;
    bool huntPaused_ = false;
    int huntPauseIndex_ = 0;
    bool huntComplete_ = false;
    int huntDefeatedCount_ = 0;
    float huntCompleteTimer_ = 0;

    // fight
    FightSim sim_;
    FightAI ai_[2];
    FightMode fightMode_ = FightMode::VsCpu;
    bool p1Ai_ = false, p2Ai_ = true;
    float simAccum_ = 0;
    bool paused_ = false;
    int pauseIndex_ = 0;
    bool showMoveList_ = false;
    int resultIndex_ = 0;
    Mesh arenaFloor_, arenaWalls_, arenaProps_, projectileMesh_;
    Camera fightCam_;
    bool fightCamInit_ = false;
    std::vector<Particle> particles_;
    std::vector<FloatText> floats_;
    std::vector<Banner> banners_;
    float hudFlash_ = 0;
    float resultTimer_ = 0;
    Rng fxRng_{42};
    float fightTime_ = 0;
    float clashPulse_ = 0;
    float slowmo_ = 1.0f;
    Vec3 standPos_[2];
    bool standPosInit_ = false;
};

} // namespace dc
