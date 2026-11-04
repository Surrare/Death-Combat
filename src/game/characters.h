// Roster and move data. Everything a fighter can do is a Move; the simulation is data-driven.
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "core/math.h"

namespace dc {

enum class StyleType { Stand, Mode };            // ASB battle styles: Stand user, or a power "Mode"
enum class Motion { None, QCF, QCB, DP };        // 236, 214, 623
enum class MoveKind { Normal, Throw, Special, HHA, GHA, Taunt };
enum class Stance { Ground, Crouch, Air };
enum class StandUse { Any, Off, On };            // move exists only with style off / on
enum class AnimKind {
    Jab, StraightR, StraightL, Hook, KickR, KickL, KneeStrike, Uppercut, Overhead, Sweep, LowKick,
    Rush, Stomp, Cast, Grab, DiveKick, Slide, Taunt, Spin, Headbutt, Lariat, Launcher
};

struct HitWindow {
    int start = 0, end = 0;           // move-relative frames, inclusive
    float damage = 50;
    int hitstun = 16, blockstun = 10;
    float knockX = 0.08f, knockY = 0.0f;   // velocity given to the victim (per frame units)
    Vec3 boxPos{0.9f, 1.2f, 0};       // hitbox center relative to attacker (x forward, y up, z lateral)
    Vec3 boxSize{0.9f, 0.6f, 1.0f};
    bool low = false, overhead = false, launcher = false, knockdown = false, hardKnockdown = false;
    int hitstop = 6;
    float chip = 0.0f;                // chip damage fraction on block
    bool standHit = false;            // attack is performed by the Stand (longer reach, drains Stand gauge on trade)
};

struct Move {
    std::string name;
    MoveKind kind = MoveKind::Normal;
    Motion motion = Motion::None;
    uint32_t buttons = 0;             // BTN_* mask that must be pressed together
    Stance stance = Stance::Ground;
    StandUse stand = StandUse::Any;
    AnimKind anim = AnimKind::Jab;
    int startup = 6, active = 3, recovery = 10;
    std::vector<HitWindow> hits;
    int chainLevel = -1;              // 0 = light, 1 = medium, 2 = heavy; normals chain upward
    bool specialCancel = true;        // can be cancelled into specials on hit/block
    float moveSpeed = 0.0f;           // forward movement per frame while moveStart<=frame<=moveEnd
    int moveStart = 0, moveEnd = 0;
    float jumpVel = 0.0f;             // vertical velocity applied at moveStart (for dive/uppercut)
    int hhgCost = 0;                  // Heart Heat bars
    int invulStart = -1, invulEnd = -1;
    bool armor = false;               // absorbs one hit during startup
    bool projectile = false;
    float projectileSpeed = 0.22f;
    Vec3 projectileSize{0.7f, 0.7f, 0.9f};
    bool rush = false;                // participates in Rush Mode clashes
    bool standAttack = false;         // Stand performs it (visual)
    bool commandThrow = false;
    bool unblockable = false;
    int totalFrames() const { return startup + active + recovery; }
};

// Visual identity. Everything is built from boxes / ellipsoids / cones in the rig renderer, no assets.
enum class HairStyle { Cropped, Spiky, LongFlow, Ponytail, Mohawk, TopKnot, Bald };
enum class StandDesign { None, Heart, Crystal, Bell, Silk };
enum Accessory : uint32_t {
    ACC_VEST = 1u << 0,        // open vest: bare chest between two cloth panels
    ACC_BARE_ARMS = 1u << 1,
    ACC_WRAPS = 1u << 2,       // bandaged forearms and fists
    ACC_GAUNTLETS = 1u << 3,   // armoured forearms
    ACC_HEADBAND = 1u << 4,    // with trailing tails
    ACC_BELT = 1u << 5,
    ACC_SASH = 1u << 6,        // knotted waist sash with hanging tails
    ACC_BOOTS = 1u << 7,
    ACC_COAT = 1u << 8,        // long coat tails and high collar
    ACC_SCARF = 1u << 9,
    ACC_VISOR = 1u << 10,      // glowing lens glasses
    ACC_GOGGLES = 1u << 11,    // goggles pushed up on the forehead
    ACC_PAULDRONS = 1u << 12,
    ACC_BEARD = 1u << 13,
    ACC_TABARD = 1u << 14,     // hanging cloth panels front and back
    ACC_APRON = 1u << 15,      // leather work apron
    ACC_SKIRT = 1u << 16,      // flared dancer skirt
    ACC_RIBBONS = 1u << 17,    // trailing wrist ribbons
    ACC_HAMMER = 1u << 18,     // big hammer slung across the back
};
struct Look {
    HairStyle hair = HairStyle::Cropped;
    StandDesign stand = StandDesign::None;
    uint32_t acc = 0;
    float shoulders = 1, torso = 1, arms = 1, legs = 1, head = 1;   // proportions, 1 = average
    Color skin{0.92f, 0.74f, 0.62f}, pants{0.15f, 0.15f, 0.18f}, boots{0.15f, 0.12f, 0.12f}, cloth{0.3f, 0.3f, 0.3f};
    bool has(Accessory a) const { return (acc & a) != 0; }
};

struct Character {
    static constexpr float BASE_HP = 1000.0f;
    std::string name;
    std::string title;
    std::string styleName;            // Stand name or Mode name
    StyleType style = StyleType::Stand;
    Color bodyColor, accentColor, hairColor, styleColor;
    Look look;
    float hp = BASE_HP;
    float walkSpeed = 0.055f, backSpeed = 0.042f;
    float jumpVel = 0.20f;
    float gravity = 0.0095f;
    float height = 1.85f;
    std::vector<Move> moves;
    std::string lore;
};

const std::vector<Character>& roster();

} // namespace dc
