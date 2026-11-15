// Scripted tour used to record the demo video: `DeathCombat --showcase --record out.mp4 30`.
// It drives the same state the keys drive (F2 mode, sun, orbit camera, pause, help) so the video shows the real controls.
#include "game/app.h"
#include "gfx/renderer.h"
#include <algorithm>
#include <cmath>

namespace dc {

namespace {
struct ShowStep {
    float t;
    int action;
};
enum Action {
    StartFightColosseum, ModeDirect, ModeNoBloom, ModeNoOutline, ModeNoShadows, ModeNoSsao, ModeNoPbr, ModeNoTextures, ModeFull,
    LightSweepStart, LightSweepEnd, OrbitOn, OrbitOff, PauseOn, PauseOff, HelpOn, HelpOff,
    FightCathedral, FightForge, GroundsOverview, End
};
const ShowStep STEPS[] = {
    {0.0f, -1},
    {4.0f, StartFightColosseum},
    {8.0f, ModeDirect}, {12.0f, ModeNoBloom}, {16.0f, ModeNoOutline}, {20.0f, ModeNoShadows},
    {24.0f, ModeNoSsao}, {28.0f, ModeNoPbr}, {32.0f, ModeNoTextures}, {36.0f, ModeFull},
    {37.0f, LightSweepStart}, {45.0f, LightSweepEnd},
    {46.0f, OrbitOn}, {56.0f, OrbitOff},
    {57.0f, PauseOn}, {60.0f, PauseOff},
    {61.0f, HelpOn}, {67.0f, HelpOff},
    {68.0f, FightCathedral}, {75.0f, FightForge}, {82.0f, GroundsOverview},
    {89.0f, End},
};
constexpr int STEP_COUNT = (int)(sizeof(STEPS) / sizeof(STEPS[0]));
}

void App::updateShowcase(float dt) {
    showT_ += dt;
    // continuous parts of the script
    const bool sweeping = showT_ >= 37.0f && showT_ < 45.0f;
    if (sweeping) renderer_.rotateLight(55.0f * dt, showT_ < 41.0f ? -8.0f * dt : 8.0f * dt);
    if (showT_ >= 46.0f && showT_ < 56.0f) orbitYaw_ += 0.45f * dt;

    while (showStep_ + 1 < STEP_COUNT && showT_ >= STEPS[showStep_ + 1].t) {
        const int a = STEPS[++showStep_].action;
        auto setMode = [&](int m, const char* caption) { settings_.renderMode = m; showCaption_ = caption; };
        switch (a) {
        case -1: showCaption_ = "Death Combat: title screen. F2 will change the render pipeline live"; break;
        case StartFightColosseum:
            startFight(0, 1, FightMode::Demo, settings_.aiDifficulty, 0);
            showCaption_ = "Fight mode, Crimson Colosseum. Full pipeline: every stage on";
            break;
        case ModeDirect: setMode((int)RenderDebugMode::Direct, "F2: DIRECT. Forward pass only, no post-processing"); break;
        case ModeNoBloom: setMode((int)RenderDebugMode::NoBloom, "F2: NO BLOOM. Bright pass and blur skipped"); break;
        case ModeNoOutline: setMode((int)RenderDebugMode::NoOutline, "F2: NO OUTLINE. Depth-edge ink line off"); break;
        case ModeNoShadows: setMode((int)RenderDebugMode::NoShadows, "F2: NO SHADOWS. Shadow map pass skipped"); break;
        case ModeNoSsao: setMode((int)RenderDebugMode::NoSsao, "F2: NO SSAO. Contact darkening under the fighters disappears"); break;
        case ModeNoPbr: setMode((int)RenderDebugMode::NoPbr, "F2: BLINN-PHONG. Cook-Torrance GGX specular switched off"); break;
        case ModeNoTextures: setMode((int)RenderDebugMode::NoTextures, "F2: NO TEXTURES. Normal-mapped brick and tile detail off"); break;
        case ModeFull: setMode((int)RenderDebugMode::Full, "F2: back to FULL. All stages on"); break;
        case LightSweepStart: showCaption_ = "[  ] and - =  rotate and tilt the sun. Shadows and highlights follow"; break;
        case LightSweepEnd: renderer_.resetLight(); showCaption_ = "0: reset the light"; break;
        case OrbitOn: {
            orbitCamActive_ = true;
            Vec3 d = renderer_.camera().eye - renderer_.camera().target;
            float len = d.length();
            if (len > 0.5f) {
                orbitDist_ = std::clamp(len, 2.0f, 35.0f);
                orbitYaw_ = std::atan2(d.x, d.z);
                orbitPitch_ = std::clamp(std::asin(std::clamp(d.y / len, -0.95f, 0.95f)), -0.25f, 1.35f);
            }
            showCaption_ = "V: orbit camera (mouse drag or arrow keys in the live build)";
            break;
        }
        case OrbitOff: orbitCamActive_ = false; showCaption_ = "V: back to the game camera"; break;
        case PauseOn: animPaused_ = true; showCaption_ = "P: pause. The scene freezes, the camera and light still work"; break;
        case PauseOff: animPaused_ = false; showCaption_ = "P: resume"; break;
        case HelpOn: showHelp_ = true; showCaption_.clear(); break;
        case HelpOff: showHelp_ = false; showCaption_.clear(); break;
        case FightCathedral:
            startFight(2, 3, FightMode::Demo, settings_.aiDifficulty, 3);
            showCaption_ = "Glass Cathedral: wide floor, point lights, shadows";
            break;
        case FightForge:
            startFight(4, 5, FightMode::Demo, settings_.aiDifficulty, 2);
            showCaption_ = "Ember Forge: lava glow and bloom";
            break;
        case GroundsOverview:
            pendingMode_ = FightMode::Hunt; selChosen_[0] = 0; selChosen_[1] = 1;
            scene_ = Scene::StageSelect; stageCursor_ = 1;
            showCaption_ = "Hunt mode: procedural labyrinths (randomized depth-first search)";
            break;
        case End: quit_ = true; break;
        default: break;
        }
    }
}

void App::drawShowcaseCaption() {
    if (!opts_.showcase) return;
    const float W = (float)width_, H = (float)height_;
    if (!showCaption_.empty() && !showHelp_) {
        const float scale = 1.7f;
        // menus keep a panel on the left, so the caption sits to its right there
        const bool menuScene = scene_ == Scene::Title || scene_ == Scene::StageSelect || scene_ == Scene::Select;
        const float boxW = menuScene ? W - 520.0f : std::min(W - 80.0f, 900.0f), boxH = 44.0f;
        const float x = menuScene ? 500.0f : (W - boxW) * 0.5f, y = H - (menuScene ? 100.0f : 118.0f);
        renderer_.rect(x, y, boxW, boxH, Color(0.02f, 0.025f, 0.035f, 0.88f));
        renderer_.rect(x, y, 4.0f, boxH, Color(0.85f, 0.72f, 0.42f, 1.0f));
        renderer_.text(x + 18.0f, y + 12.0f, showCaption_, scale, Color(1, 1, 1, 1));
    }
}

} // namespace dc
