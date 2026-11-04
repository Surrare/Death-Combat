// Fight arenas: round floors you can walk freely on, each with its own geometry, lighting and ambient effects.
// Everything is procedural; tall scenery stays outside the camera radius so it never blocks the view.
#pragma once
#include "core/math.h"
#include "gfx/mesh.h"
#include "gfx/renderer.h"
#include "game/fight.h"

namespace dc {

constexpr int ARENA_COUNT = 4;

struct ArenaInfo {
    const char* name;
    const char* blurb;
    ArenaShape shape;      // what the simulation uses
    float cameraRadius;    // the fight camera's eye is kept inside this circle
    Color clear;           // sky / background colour
};

const ArenaInfo& arenaInfo(int id);
int homeArena(int characterId);   // where a fighter waits for you in Hunt mode

class Arena {
public:
    void build(int id);   // needs a GL context
    int id() const { return id_; }
    const ArenaInfo& info() const { return arenaInfo(id_); }
    // Sets sun, fog, rim and the arena's point lights, then submits geometry and ambient effects.
    // accentA / accentB are the two fighters' colours, used for banners and pillar strips.
    void render(Renderer& r, const Mesh& unitBox, float time, Color accentA, Color accentB) const;

private:
    int id_ = 0;
    Mesh ground_, dais_, walls_, props_, glow_, pulse_;
};

} // namespace dc
