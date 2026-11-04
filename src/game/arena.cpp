#include "game/arena.h"
#include <cmath>

namespace dc {

namespace {

const ArenaInfo ARENAS[ARENA_COUNT] = {
    {"CRIMSON COLOSSEUM", "A torch-lit hall of brick and banners. The crowd is long gone; the blood stays.",
     {9.0f, 4.4f}, 14.0f, Color(0.05f, 0.04f, 0.06f)},
    {"MOONLIT SHRINE", "A stone courtyard under a full moon, ringed by lanterns and falling blossoms.",
     {9.5f, 4.4f}, 14.5f, Color(0.04f, 0.05f, 0.11f)},
    {"EMBER FORGE", "A cavern foundry on an island of rock, surrounded by a moat of molten iron.",
     {9.0f, 4.4f}, 13.5f, Color(0.07f, 0.025f, 0.02f)},
    {"GLASS CATHEDRAL", "Stained glass throws coloured light across a marble floor. Pray quietly.",
     {9.0f, 4.4f}, 14.0f, Color(0.05f, 0.045f, 0.08f)},
};

// ------------------------------------------------------------ mesh helpers ----
// Local +z of the returned frame points away from the arena centre.
Mat4 polar(float angle, float radius, float y) {
    return Mat4::translate(Vec3(std::sin(angle) * radius, y, std::cos(angle) * radius)) * Mat4::rotateY(angle);
}
void boxAt(MeshBuilder& b, const Mat4& m, Vec3 size, Vec3 col) {
    MeshBuilder t;
    t.addBox(Vec3(0, 0, 0), size, col);
    b.append(t, m);
}
// cone along +y (apex up) or hanging down when down = true
void coneAt(MeshBuilder& b, Vec3 base, float r, float h, int seg, Vec3 col, bool down = false) {
    Vec3 apex = base + Vec3(0, down ? -h : h, 0);
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * 2 * PI, a1 = (float)(i + 1) / seg * 2 * PI;
        Vec3 p0 = base + Vec3(std::cos(a0) * r, 0, std::sin(a0) * r), p1 = base + Vec3(std::cos(a1) * r, 0, std::sin(a1) * r);
        if (!down) {
            Vec3 n = normalize(cross(apex - p0, p1 - p0));
            b.addTriangle({p0, n, col}, {apex, n, col}, {p1, n, col});
        } else {
            Vec3 n = normalize(cross(p1 - p0, apex - p0));
            b.addTriangle({p0, n, col}, {p1, n, col}, {apex, n, col});
        }
    }
}
// flat annulus facing up
void ringAt(MeshBuilder& b, float r0, float r1, float y, Vec3 col, int seg) {
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * 2 * PI, a1 = (float)(i + 1) / seg * 2 * PI;
        Vec3 p0(std::cos(a0) * r0, y, std::sin(a0) * r0), p1(std::cos(a0) * r1, y, std::sin(a0) * r1);
        Vec3 p2(std::cos(a1) * r1, y, std::sin(a1) * r1), p3(std::cos(a1) * r0, y, std::sin(a1) * r0);
        b.addQuad(p0, p3, p2, p1, col);
    }
}
// disk platform whose top surface sits at topY
void daisAt(MeshBuilder& b, float r, float topY, float thick, Vec3 col) {
    b.addCylinder(Vec3(0, topY - thick, 0), r, thick, 64, col);
}

float hash1(int i, int salt) {
    uint32_t h = (uint32_t)i * 374761393u + (uint32_t)salt * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0f;
}

const float SURFACE = 0.01f;   // fighters' feet (App::ARENA_SURFACE_Y)

// ------------------------------------------------------------ arena builds ----
void buildColosseum(MeshBuilder& ground, MeshBuilder& dais, MeshBuilder& walls, MeshBuilder& props, MeshBuilder& glow) {
    ground.addPlane(Vec3(0, 0, 0), 70, 70, 35, Vec3(0.22f, 0.2f, 0.22f), Vec3(0.19f, 0.17f, 0.2f));
    daisAt(dais, 10.4f, SURFACE, 0.03f, Vec3(0.34f, 0.3f, 0.3f));
    // ring emblem and the edge of the fighting floor
    const Vec3 gold(0.62f, 0.45f, 0.16f), crimson(0.5f, 0.08f, 0.11f);
    ringAt(glow, 3.58f, 3.66f, SURFACE + 0.006f, gold, 72);
    ringAt(glow, 3.22f, 3.26f, SURFACE + 0.006f, crimson, 72);
    ringAt(glow, 1.15f, 1.2f, SURFACE + 0.006f, gold, 48);
    for (int k = 0; k < 24; ++k) {
        float a = (float)k / 24 * 2 * PI;
        Vec3 c(std::cos(a) * 3.4f, SURFACE + 0.006f, std::sin(a) * 3.4f), t(-std::sin(a), 0, std::cos(a)), r(std::cos(a), 0, std::sin(a));
        glow.addQuad(c - r * 0.16f - t * 0.025f, c + r * 0.16f - t * 0.025f, c + r * 0.16f + t * 0.025f, c - r * 0.16f + t * 0.025f, gold);
    }
    ringAt(glow, 8.9f, 9.05f, SURFACE + 0.006f, crimson, 96);
    // outer wall and a ring of pillars
    for (int k = 0; k < 28; ++k) {
        float a = (float)k / 28 * 2 * PI;
        boxAt(walls, polar(a, 21.0f, 4.5f), Vec3(4.9f, 9.0f, 1.2f), Vec3(0.36f, 0.32f, 0.34f));
        boxAt(walls, polar(a, 20.5f, 0.3f), Vec3(4.9f, 0.6f, 0.6f), Vec3(0.14f, 0.12f, 0.12f));
    }
    for (int k = 0; k < 12; ++k) {
        float a = (k + 0.5f) / 12 * 2 * PI;
        boxAt(walls, polar(a, 16.5f, 4.0f), Vec3(1.1f, 8.0f, 1.1f), Vec3(0.45f, 0.42f, 0.44f));
        boxAt(walls, polar(a, 16.5f, 0.3f), Vec3(1.6f, 0.6f, 1.6f), Vec3(0.3f, 0.28f, 0.3f));
        boxAt(walls, polar(a, 16.5f, 7.9f), Vec3(1.6f, 0.5f, 1.6f), Vec3(0.3f, 0.28f, 0.3f));
    }
    // banners on the wall between the pillars
    for (int k = 0; k < 12; ++k) {
        float a = (float)k / 12 * 2 * PI;
        Vec3 cloth = (k & 1) ? Vec3(0.30f, 0.04f, 0.07f) : Vec3(0.10f, 0.06f, 0.22f);
        const Vec3 trim(0.85f, 0.65f, 0.22f);
        boxAt(props, polar(a, 20.3f, 5.0f), Vec3(2.0f, 4.0f, 0.06f), cloth);
        boxAt(props, polar(a, 20.25f, 7.1f), Vec3(2.4f, 0.14f, 0.14f), trim);
        boxAt(props, polar(a, 20.25f, 3.05f), Vec3(2.0f, 0.1f, 0.08f), trim);
        for (int t = 0; t < 6; ++t)
            boxAt(props, polar(a, 20.25f, 2.85f) * Mat4::translate(Vec3(-0.85f + t * 0.34f, 0, 0)), Vec3(0.07f, 0.32f, 0.05f), trim);
        boxAt(props, polar(a, 20.24f, 5.2f) * Mat4::rotateZ(PI * 0.25f), Vec3(0.6f, 0.6f, 0.05f), trim);
        boxAt(props, polar(a, 20.22f, 5.2f) * Mat4::rotateZ(PI * 0.25f), Vec3(0.32f, 0.32f, 0.06f), cloth);
    }
    // braziers just outside the camera's path
    for (int k = 0; k < 4; ++k) {
        float a = (k + 0.25f) / 4 * 2 * PI;
        Vec3 p(std::sin(a) * 14.8f, 0, std::cos(a) * 14.8f);
        props.addCylinder(p, 0.45f, 1.3f, 12, Vec3(0.2f, 0.18f, 0.16f));
        props.addCylinder(p + Vec3(0, 1.3f, 0), 0.7f, 0.3f, 12, Vec3(0.3f, 0.25f, 0.2f));
    }
}

void buildShrine(MeshBuilder& ground, MeshBuilder& dais, MeshBuilder& walls, MeshBuilder& props, MeshBuilder& glow) {
    ground.addPlane(Vec3(0, 0, 0), 300, 300, 60, Vec3(0.09f, 0.13f, 0.1f), Vec3(0.08f, 0.12f, 0.09f));
    daisAt(dais, 11.0f, SURFACE, 0.05f, Vec3(0.38f, 0.37f, 0.4f));
    daisAt(dais, 11.8f, SURFACE - 0.05f, 0.04f, Vec3(0.27f, 0.26f, 0.29f));
    ringAt(glow, 9.4f, 9.5f, SURFACE + 0.006f, Vec3(0.45f, 0.5f, 0.8f), 96);
    // moss-and-stone mandala in the middle
    ringAt(glow, 2.4f, 2.48f, SURFACE + 0.006f, Vec3(0.55f, 0.6f, 0.9f), 64);
    for (int k = 0; k < 8; ++k) {
        float a = (float)k / 8 * 2 * PI;
        Mat4 m = Mat4::translate(Vec3(std::sin(a) * 1.4f, SURFACE + 0.005f, std::cos(a) * 1.4f)) * Mat4::rotateY(a) * Mat4::rotateY(PI * 0.25f);
        boxAt(glow, m, Vec3(0.45f, 0.004f, 0.45f), Vec3(0.4f, 0.45f, 0.75f));
    }
    // torii gates beyond the lanterns
    const Vec3 vermilion(0.75f, 0.15f, 0.1f), black(0.12f, 0.08f, 0.08f);
    for (int k = 0; k < 4; ++k) {
        float a = (k + 0.5f) / 4 * 2 * PI;
        Mat4 g = polar(a, 20.0f, 0);
        for (int s = -1; s <= 1; s += 2) {
            MeshBuilder c; c.addCylinder(Vec3(0, 0, 0), 0.35f, 6.2f, 14, vermilion);
            walls.append(c, g * Mat4::translate(Vec3(s * 2.3f, 0, 0)));
            boxAt(walls, g * Mat4::translate(Vec3(s * 2.3f, 0.25f, 0)), Vec3(0.9f, 0.5f, 0.9f), black);
        }
        boxAt(walls, g * Mat4::translate(Vec3(0, 6.35f, 0)), Vec3(7.2f, 0.5f, 0.6f), black);
        boxAt(walls, g * Mat4::translate(Vec3(0, 5.95f, 0)), Vec3(6.6f, 0.35f, 0.5f), vermilion);
        boxAt(walls, g * Mat4::translate(Vec3(0, 5.0f, 0)), Vec3(5.6f, 0.3f, 0.35f), vermilion);
        boxAt(walls, g * Mat4::translate(Vec3(0, 5.5f, 0)), Vec3(0.5f, 0.8f, 0.2f), black);
    }
    // stone lanterns
    for (int k = 0; k < 16; ++k) {
        float a = (float)k / 16 * 2 * PI;
        Mat4 m = polar(a, 15.5f, 0);
        const Vec3 stone(0.4f, 0.39f, 0.42f);
        boxAt(props, m * Mat4::translate(Vec3(0, 0.12f, 0)), Vec3(0.7f, 0.25f, 0.7f), stone);
        boxAt(props, m * Mat4::translate(Vec3(0, 0.75f, 0)), Vec3(0.22f, 1.0f, 0.22f), stone);
        boxAt(props, m * Mat4::translate(Vec3(0, 1.32f, 0)), Vec3(0.6f, 0.14f, 0.6f), stone);
        boxAt(props, m * Mat4::translate(Vec3(0, 1.85f, 0)), Vec3(0.6f, 0.12f, 0.6f), stone);
        boxAt(glow, m * Mat4::translate(Vec3(0, 1.6f, 0)), Vec3(0.42f, 0.36f, 0.42f), Vec3(1.0f, 0.72f, 0.38f));
        Vec3 c = m.transformPoint(Vec3(0, 1.9f, 0));
        coneAt(props, c, 0.5f, 0.38f, 8, stone);
    }
    // cherry trees and distant mountains
    Rng rng(4242);
    for (int k = 0; k < 14; ++k) {
        float a = (k + rng.uniform(-0.3f, 0.3f)) / 14 * 2 * PI, rad = rng.uniform(24.0f, 31.0f);
        Vec3 p(std::sin(a) * rad, 0, std::cos(a) * rad);
        const Vec3 bark(0.18f, 0.11f, 0.1f);
        boxAt(props, Mat4::translate(p + Vec3(0, 1.6f, 0)) * Mat4::rotateZ(rng.uniform(-0.1f, 0.1f)), Vec3(0.45f, 3.2f, 0.45f), bark);
        for (int s = 0; s < 3; ++s)
            boxAt(props, Mat4::translate(p + Vec3(0, 3.0f, 0)) * Mat4::rotateY(s * 2.1f) * Mat4::rotateZ(0.7f) * Mat4::translate(Vec3(0, 0.9f, 0)), Vec3(0.22f, 1.8f, 0.22f), bark);
        for (int s = 0; s < 5; ++s) {
            Vec3 o(rng.uniform(-1.6f, 1.6f), rng.uniform(3.6f, 5.0f), rng.uniform(-1.6f, 1.6f));
            float rr = rng.uniform(1.0f, 1.7f);
            props.addSphere(p + o, rr, 7, 10, Vec3(0.86f, 0.46f, 0.62f) * rng.uniform(0.85f, 1.1f));
        }
    }
    for (int k = 0; k < 12; ++k) {   // distant hills, close enough to show through the night haze
        float a = (k + rng.uniform(0, 0.6f)) / 12 * 2 * PI, rad = rng.uniform(55.0f, 72.0f);
        coneAt(walls, Vec3(std::sin(a) * rad, -1, std::cos(a) * rad), rng.uniform(14, 22), rng.uniform(14, 28), 7, Vec3(0.07f, 0.08f, 0.14f));
    }
}

void buildForge(MeshBuilder& ground, MeshBuilder& dais, MeshBuilder& walls, MeshBuilder& props, MeshBuilder& glow, MeshBuilder& pulse) {
    ground.addPlane(Vec3(0, -0.08f, 0), 140, 140, 40, Vec3(0.07f, 0.06f, 0.06f), Vec3(0.08f, 0.065f, 0.06f));
    daisAt(dais, 10.4f, SURFACE, 0.4f, Vec3(0.2f, 0.15f, 0.13f));
    // molten moat and glowing cracks
    ringAt(pulse, 10.3f, 15.2f, -0.05f, Vec3(1.0f, 0.42f, 0.1f), 96);
    ringAt(dais, 15.2f, 16.0f, -0.04f, Vec3(0.12f, 0.09f, 0.08f), 96);
    Rng rng(777);
    for (int k = 0; k < 22; ++k) {
        float a = rng.uniform(0, 2 * PI), r0 = rng.uniform(1.0f, 7.5f), len = rng.uniform(0.8f, 2.6f);
        Vec3 d(std::cos(a), 0, std::sin(a)), t(-d.z, 0, d.x);
        Vec3 p0 = d * r0 + Vec3(0, SURFACE + 0.004f, 0), p1 = p0 + (d + t * rng.uniform(-0.6f, 0.6f)) * len;
        Vec3 side = normalize(cross(p1 - p0, Vec3(0, 1, 0))) * 0.03f;
        glow.addQuad(p0 + side, p1 + side, p1 - side, p0 - side, Vec3(1.0f, 0.45f, 0.12f));
    }
    ringAt(glow, 8.9f, 9.0f, SURFACE + 0.006f, Vec3(1.0f, 0.5f, 0.15f), 96);
    // jagged cavern wall and stalactites
    for (int k = 0; k < 40; ++k) {
        float a = (float)k / 40 * 2 * PI + rng.uniform(-0.05f, 0.05f), rad = rng.uniform(22.0f, 25.0f);
        float h = rng.uniform(7.0f, 17.0f), w = rng.uniform(3.0f, 5.0f);
        Mat4 m = polar(a, rad, h * 0.5f - 0.5f) * Mat4::rotateY(rng.uniform(-0.4f, 0.4f)) * Mat4::rotateZ(rng.uniform(-0.15f, 0.15f));
        boxAt(walls, m, Vec3(w, h, rng.uniform(2.5f, 4.0f)), Vec3(0.14f, 0.11f, 0.1f) * rng.uniform(0.75f, 1.2f));
    }
    for (int k = 0; k < 36; ++k) {
        float a = rng.uniform(0, 2 * PI), rad = rng.uniform(12.0f, 23.0f);
        coneAt(walls, Vec3(std::sin(a) * rad, 17.0f, std::cos(a) * rad), rng.uniform(0.6f, 1.4f), rng.uniform(2.0f, 6.0f), 6, Vec3(0.12f, 0.09f, 0.08f), true);
    }
    // anvils and furnaces past the moat
    for (int k = 0; k < 4; ++k) {
        float a = (k + 0.5f) / 4 * 2 * PI;
        Mat4 m = polar(a, 16.6f, 0);
        const Vec3 iron(0.22f, 0.22f, 0.25f);
        boxAt(props, m * Mat4::translate(Vec3(0, 0.25f, 0)), Vec3(0.55f, 0.5f, 0.4f), iron);
        boxAt(props, m * Mat4::translate(Vec3(0, 0.62f, 0)), Vec3(1.15f, 0.32f, 0.45f), iron);
        Mat4 f = polar(a + PI / 4, 18.5f, 0);
        boxAt(props, f * Mat4::translate(Vec3(0, 1.8f, 0)), Vec3(2.2f, 3.6f, 2.2f), Vec3(0.2f, 0.13f, 0.11f));
        boxAt(props, f * Mat4::translate(Vec3(0, 5.0f, 0)), Vec3(0.9f, 3.0f, 0.9f), Vec3(0.16f, 0.11f, 0.1f));
        boxAt(glow, f * Mat4::translate(Vec3(0, 1.2f, -1.11f)), Vec3(1.1f, 0.9f, 0.05f), Vec3(1.0f, 0.55f, 0.15f));
    }
    // chains hanging from the dark
    for (int k = 0; k < 6; ++k) {
        float a = (k + 0.2f) / 6 * 2 * PI;
        for (int l = 0; l < 14; ++l)
            boxAt(props, polar(a, 12.5f, 16.5f - l * 0.5f) * Mat4::rotateY(l % 2 ? PI * 0.5f : 0.0f), Vec3(0.18f, 0.42f, 0.05f), Vec3(0.18f, 0.17f, 0.18f));
    }
}

const Vec3 GLASS[5] = {{0.2f, 0.35f, 1.0f}, {0.9f, 0.2f, 0.25f}, {1.0f, 0.75f, 0.25f}, {0.6f, 0.3f, 1.0f}, {0.2f, 0.8f, 0.5f}};

void buildCathedral(MeshBuilder& ground, MeshBuilder& dais, MeshBuilder& walls, MeshBuilder& props, MeshBuilder& glow) {
    ground.addPlane(Vec3(0, 0, 0), 60, 60, 30, Vec3(0.55f, 0.53f, 0.58f), Vec3(0.16f, 0.15f, 0.2f));
    daisAt(dais, 10.2f, SURFACE, 0.06f, Vec3(0.42f, 0.4f, 0.46f));
    // rosette inlay
    // floor inlays sit in the bright glow mesh with the windows, so their colours are kept dim
    ringAt(glow, 3.0f, 3.08f, SURFACE + 0.006f, Vec3(0.5f, 0.38f, 0.15f), 72);
    ringAt(glow, 1.0f, 1.05f, SURFACE + 0.006f, Vec3(0.5f, 0.38f, 0.15f), 48);
    for (int k = 0; k < 16; ++k) {
        float a = (float)k / 16 * 2 * PI;
        Vec3 d(std::sin(a), 0, std::cos(a)), t(-d.z, 0, d.x), y(0, SURFACE + 0.005f, 0);
        glow.addQuad(y + d * 1.1f, y + d * 2.0f + t * 0.3f, y + d * 2.9f, y + d * 2.0f - t * 0.3f, GLASS[k % 2 ? 0 : 3] * 0.35f);
    }
    ringAt(glow, 8.9f, 9.0f, SURFACE + 0.006f, Vec3(0.4f, 0.34f, 0.6f), 96);
    // wall ring with stained glass windows and tall pillars
    for (int k = 0; k < 28; ++k) {
        float a = (float)k / 28 * 2 * PI;
        boxAt(walls, polar(a, 20.5f, 8.0f), Vec3(4.8f, 16.0f, 1.2f), Vec3(0.42f, 0.4f, 0.45f));
    }
    Rng rng(31);
    for (int k = 0; k < 12; ++k) {
        float a = (float)k / 12 * 2 * PI;
        Mat4 w = polar(a, 19.85f, 0);
        boxAt(props, w * Mat4::translate(Vec3(0, 7.0f, 0)), Vec3(2.5f, 7.6f, 0.08f), Vec3(0.08f, 0.07f, 0.1f));   // lead frame
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 2; ++col)
                boxAt(glow, w * Mat4::translate(Vec3(-0.55f + col * 1.1f, 3.9f + row * 1.4f, -0.05f)), Vec3(0.95f, 1.25f, 0.04f), GLASS[(k + row * 2 + col + rng.range(0, 1)) % 5]);
        boxAt(glow, w * Mat4::translate(Vec3(0, 11.2f, -0.05f)) * Mat4::rotateZ(PI * 0.25f), Vec3(1.3f, 1.3f, 0.04f), GLASS[k % 5]);
    }
    for (int k = 0; k < 12; ++k) {
        float a = (k + 0.5f) / 12 * 2 * PI;
        Mat4 m = polar(a, 16.2f, 0);
        MeshBuilder c; c.addCylinder(Vec3(0, 0, 0), 0.75f, 15.0f, 16, Vec3(0.55f, 0.53f, 0.58f));
        walls.append(c, m);
        boxAt(walls, m * Mat4::translate(Vec3(0, 0.4f, 0)), Vec3(2.0f, 0.8f, 2.0f), Vec3(0.4f, 0.38f, 0.43f));
        boxAt(walls, m * Mat4::translate(Vec3(0, 14.8f, 0)), Vec3(2.0f, 0.6f, 2.0f), Vec3(0.4f, 0.38f, 0.43f));
    }
    // candelabras and a chandelier
    for (int k = 0; k < 8; ++k) {
        float a = (k + 0.5f) / 8 * 2 * PI;
        Mat4 m = polar(a, 14.8f, 0);
        boxAt(props, m * Mat4::translate(Vec3(0, 0.9f, 0)), Vec3(0.08f, 1.8f, 0.08f), Vec3(0.5f, 0.4f, 0.2f));
        boxAt(props, m * Mat4::translate(Vec3(0, 1.8f, 0)), Vec3(0.7f, 0.05f, 0.08f), Vec3(0.5f, 0.4f, 0.2f));
        for (int c = -1; c <= 1; ++c) boxAt(glow, m * Mat4::translate(Vec3(c * 0.3f, 1.95f, 0)), Vec3(0.06f, 0.22f, 0.06f), Vec3(1.0f, 0.9f, 0.7f));
    }
    for (int k = 0; k < 18; ++k) {
        float a = (float)k / 18 * 2 * PI;
        glow.addSphere(Vec3(std::sin(a) * 4.5f, 13.0f, std::cos(a) * 4.5f), 0.18f, 6, 8, Vec3(1.0f, 0.85f, 0.55f));
    }
    ringAt(props, 4.3f, 4.7f, 13.0f, Vec3(0.45f, 0.36f, 0.18f), 48);
}

} // namespace

const ArenaInfo& arenaInfo(int id) { return ARENAS[((id % ARENA_COUNT) + ARENA_COUNT) % ARENA_COUNT]; }

int homeArena(int characterId) {
    static const int HOME[] = {0, 3, 3, 1, 1, 2};   // KAIN, VESPER, ORSOLO, MIRA, KAZE, BRAM
    return characterId >= 0 && characterId < 6 ? HOME[characterId] : 0;
}

void Arena::build(int id) {
    id_ = ((id % ARENA_COUNT) + ARENA_COUNT) % ARENA_COUNT;
    MeshBuilder ground, dais, walls, props, glow, pulse;
    switch (id_) {
    case 0: buildColosseum(ground, dais, walls, props, glow); break;
    case 1: buildShrine(ground, dais, walls, props, glow); break;
    case 2: buildForge(ground, dais, walls, props, glow, pulse); break;
    default: buildCathedral(ground, dais, walls, props, glow); break;
    }
    ground_.upload(ground); dais_.upload(dais); walls_.upload(walls); props_.upload(props); glow_.upload(glow); pulse_.upload(pulse);
}

void Arena::render(Renderer& r, const Mesh& unitBox, float t, Color accentA, Color accentB) const {
    auto glowAt = [&](Vec3 p, float size, Color c, float rot = 0) { r.drawBillboard(p, size, c, Blend::Additive, rot); };
    // shafts of light: stacked soft glows between two points
    auto shaft = [&](Vec3 top, Vec3 bottom, float w0, float w1, Color c, int n) {
        for (int s = 0; s < n; ++s) {
            float k = (float)s / (float)(n - 1);
            glowAt(lerp(top, bottom, k), lerp(w0, w1, k), c.withAlpha(c.a * (0.6f + 0.4f * (1.0f - k))));
        }
    };
    switch (id_) {
    case 0: {   // ---------------------------------------------------- colosseum
        r.setDirectionalLight(Vec3(-0.35f, -1.0f, -0.45f), Vec3(1.0f, 0.95f, 0.88f) * 0.9f, 0.32f);
        r.setFog(Vec3(0.05f, 0.04f, 0.06f), 0.0015f);
        r.setRimColor(Vec3(0.45f, 0.6f, 1.0f));
        for (int k = 0; k < 4; ++k) {
            float a = (k + 0.25f) / 4 * 2 * PI;
            Vec3 p(std::sin(a) * 14.8f, 1.9f, std::cos(a) * 14.8f);
            r.addPointLight(p, Vec3(1.0f, 0.55f, 0.2f) * (1.0f + 0.06f * std::sin(t * 3.0f + k)), 10);
            glowAt(p + Vec3(0, 0.15f * std::sin(t * 12 + k), 0), 1.4f, Color(1.0f, 0.55f, 0.15f, 0.6f), t * 3);
            glowAt(p, 0.7f, Color(1.0f, 0.9f, 0.5f, 0.9f), -t * 2);
        }
        r.draw(ground_, Mat4::identity(), Color(1, 1, 1), 0, 1);
        r.draw(dais_, Mat4::identity(), Color(1, 1, 1), 0, 1);
        r.draw(walls_, Mat4::identity(), Color(1, 1, 1), 0, 2);
        r.draw(props_, Mat4::identity(), Color(1, 1, 1), 0.04f, 0);
        r.draw(glow_, Mat4::identity(), Color(1, 1, 1), 0.5f, 0, false);
        for (int k = 0; k < 12; ++k) {   // pillar strips in the fighters' colours
            float a = (k + 0.5f) / 12 * 2 * PI;
            r.draw(unitBox, polar(a, 16.5f - 0.58f, 4.0f) * Mat4::scale(Vec3(0.3f, 5.0f, 0.05f)), (k % 2 ? accentB : accentA) * 0.9f, 0.9f, 0, false);
        }
        for (int k = 0; k < 6; ++k) {
            float a = (float)k / 6 * 2 * PI + 0.3f;
            shaft(Vec3(std::sin(a) * 9.0f, 12.0f, std::cos(a) * 9.0f), Vec3(std::sin(a) * 4.5f, 0.0f, std::cos(a) * 4.5f), 1.6f, 4.0f, Color(1.0f, 0.88f, 0.7f, 0.022f), 14);
        }
        for (int i = 0; i < 50; ++i) {   // dust motes
            float y = std::fmod(hash1(i, 1) * 6.0f + t * (0.05f + 0.1f * hash1(i, 2)), 6.0f);
            Vec3 p((hash1(i, 3) - 0.5f) * 24.0f + 0.3f * std::sin(t * 0.4f + i), y, (hash1(i, 4) - 0.5f) * 24.0f);
            glowAt(p, 0.035f, Color(1.0f, 0.9f, 0.7f, 0.45f * std::sin(PI * y / 6.0f)));
        }
        break;
    }
    case 1: {   // ------------------------------------------------------ shrine
        r.setDirectionalLight(Vec3(0.45f, -1.0f, 0.55f), Vec3(0.55f, 0.62f, 0.95f) * 0.85f, 0.4f);
        r.setFog(Vec3(0.05f, 0.06f, 0.12f), 0.0005f);
        r.setRimColor(Vec3(0.6f, 0.72f, 1.0f));
        for (int k = 0; k < 6; ++k) {
            float a = (float)k / 6 * 2 * PI;
            r.addPointLight(Vec3(std::sin(a) * 15.5f, 1.6f, std::cos(a) * 15.5f), Vec3(1.0f, 0.7f, 0.4f) * 1.1f, 9);
        }
        r.draw(ground_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(dais_, Mat4::identity(), Color(1, 1, 1), 0, 1);
        r.draw(walls_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(props_, Mat4::identity(), Color(1, 1, 1), 0.03f, 0);
        r.draw(glow_, Mat4::identity(), Color(1, 1, 1), 0.85f, 0, false);
        for (int k = 0; k < 16; ++k) {
            float a = (float)k / 16 * 2 * PI;
            glowAt(Vec3(std::sin(a) * 15.5f, 1.6f, std::cos(a) * 15.5f), 1.3f, Color(1.0f, 0.7f, 0.35f, 0.35f));
        }
        // the moon, its halo and a field of stars
        Vec3 moon(-90, 70, -120);
        glowAt(moon, 120.0f, Color(0.5f, 0.6f, 1.0f, 0.12f));
        glowAt(moon, 30.0f, Color(0.95f, 0.95f, 1.0f, 0.95f));
        glowAt(moon, 22.0f, Color(1.0f, 1.0f, 1.0f, 0.9f));
        for (int i = 0; i < 140; ++i) {
            float a = hash1(i, 5) * 2 * PI, el = 0.12f + hash1(i, 6) * 1.2f;
            Vec3 d(std::cos(el) * std::sin(a), std::sin(el), std::cos(el) * std::cos(a));
            glowAt(d * 150.0f, 0.7f + hash1(i, 7) * 0.8f, Color(0.9f, 0.92f, 1.0f, 0.5f + 0.4f * std::sin(t * (1.0f + hash1(i, 8) * 3.0f) + i)));
        }
        for (int i = 0; i < 110; ++i) {   // falling blossom petals
            float fall = 0.35f + 0.25f * hash1(i, 9);
            float y = 9.0f - std::fmod(hash1(i, 10) * 9.0f + t * fall, 9.0f);
            Vec3 p((hash1(i, 11) - 0.5f) * 26.0f + std::sin(t * 0.8f + i) * 0.6f, y, (hash1(i, 12) - 0.5f) * 26.0f + std::cos(t * 0.6f + i * 1.3f) * 0.6f);
            r.drawBillboard(p, 0.09f, Color(1.0f, 0.62f, 0.78f, 0.85f), Blend::Alpha, t * 2.0f + i);
        }
        break;
    }
    case 2: {   // ------------------------------------------------------- forge
        float throb = 0.85f + 0.15f * std::sin(t * 1.3f) + 0.05f * std::sin(t * 4.7f);
        r.setDirectionalLight(Vec3(0.2f, -1.0f, 0.25f), Vec3(0.85f, 0.5f, 0.35f) * 0.45f, 0.3f);
        r.setFog(Vec3(0.12f, 0.04f, 0.02f), 0.0022f);
        r.setRimColor(Vec3(1.0f, 0.55f, 0.3f));
        for (int k = 0; k < 6; ++k) {
            float a = (float)k / 6 * 2 * PI;
            r.addPointLight(Vec3(std::sin(a) * 12.7f, 0.7f, std::cos(a) * 12.7f), Vec3(1.0f, 0.45f, 0.15f) * (1.3f * throb), 10);
        }
        for (int k = 0; k < 4; ++k) {
            float a = (k + 0.5f) / 4 * 2 * PI + PI / 4;
            r.addPointLight(Vec3(std::sin(a) * 17.0f, 1.3f, std::cos(a) * 17.0f), Vec3(1.0f, 0.5f, 0.15f) * 0.9f, 7);
        }
        r.draw(ground_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(dais_, Mat4::identity(), Color(1, 1, 1), 0, 1);
        r.draw(walls_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(props_, Mat4::identity(), Color(1, 1, 1), 0, 3);
        r.draw(glow_, Mat4::identity(), Color(1, 1, 1), 0.9f, 0, false);
        r.draw(pulse_, Mat4::identity(), Color(throb, throb * 0.95f, throb * 0.9f), 1.0f, 0, false);
        for (int i = 0; i < 120; ++i) {   // embers rising from the moat
            float rise = 0.6f + 0.8f * hash1(i, 13);
            float y = std::fmod(hash1(i, 14) * 10.0f + t * rise, 10.0f);
            float a = hash1(i, 15) * 2 * PI, rad = 10.5f + hash1(i, 16) * 4.5f - y * 0.15f;
            Vec3 p(std::sin(a) * rad + std::sin(t + i) * 0.3f, y, std::cos(a) * rad);
            glowAt(p, 0.07f, Color(1.0f, 0.55f, 0.2f, 0.9f * (1.0f - y / 10.0f)));
        }
        for (int k = 0; k < 10; ++k) {   // heat haze glow above the moat
            float a = (float)k / 10 * 2 * PI + t * 0.05f;
            glowAt(Vec3(std::sin(a) * 12.7f, 0.6f, std::cos(a) * 12.7f), 6.0f, Color(1.0f, 0.4f, 0.1f, 0.06f * throb));
        }
        break;
    }
    default: {  // --------------------------------------------------- cathedral
        r.setDirectionalLight(Vec3(0.25f, -1.0f, 0.15f), Vec3(0.8f, 0.8f, 1.0f) * 0.55f, 0.42f);
        r.setFog(Vec3(0.07f, 0.06f, 0.11f), 0.0012f);
        r.setRimColor(Vec3(0.8f, 0.75f, 1.0f));
        for (int k = 0; k < 6; ++k) {
            float a = (float)(k * 2) / 12 * 2 * PI;
            r.addPointLight(Vec3(std::sin(a) * 16.0f, 7.0f, std::cos(a) * 16.0f), GLASS[k % 5] * 0.9f, 14);
        }
        r.addPointLight(Vec3(0, 11.0f, 0), Vec3(1.0f, 0.85f, 0.6f) * 0.7f, 14);
        r.draw(ground_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(dais_, Mat4::identity(), Color(1, 1, 1), 0, 0);
        r.draw(walls_, Mat4::identity(), Color(1, 1, 1), 0, 2);
        r.draw(props_, Mat4::identity(), Color(1, 1, 1), 0.05f, 0);
        r.draw(glow_, Mat4::identity(), Color(1, 1, 1), 0.95f, 0, false);
        for (int k = 0; k < 12; ++k) {   // coloured light from each window falling across the floor
            float a = (float)k / 12 * 2 * PI;
            Vec3 c = GLASS[k % 5];
            shaft(Vec3(std::sin(a) * 19.0f, 9.0f, std::cos(a) * 19.0f), Vec3(std::sin(a) * 7.5f, 0.1f, std::cos(a) * 7.5f), 2.4f, 3.6f, Color(c.x, c.y, c.z, 0.028f), 16);
        }
        for (int k = 0; k < 18; ++k) {
            float a = (float)k / 18 * 2 * PI;
            glowAt(Vec3(std::sin(a) * 4.5f, 13.0f, std::cos(a) * 4.5f), 0.9f, Color(1.0f, 0.85f, 0.5f, 0.45f + 0.1f * std::sin(t * 7 + k)));
        }
        for (int i = 0; i < 50; ++i) {   // dust drifting in the coloured light
            float y = std::fmod(hash1(i, 17) * 8.0f + t * 0.08f, 8.0f);
            Vec3 p((hash1(i, 18) - 0.5f) * 22.0f, y, (hash1(i, 19) - 0.5f) * 22.0f);
            glowAt(p, 0.04f, Color(0.9f, 0.85f, 1.0f, 0.5f * std::sin(PI * y / 8.0f)));
        }
        break;
    }
    }
}

} // namespace dc
