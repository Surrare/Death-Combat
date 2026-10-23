// Deterministic fixed-step (60 Hz) fight simulation with JoJo ASB-style mechanics.
#pragma once
#include <vector>
#include <string>
#include "core/math.h"
#include "game/characters.h"
#include "game/input.h"

namespace dc {

enum class FState {
    Intro, Idle, Walk, Crouch, Jump, Land, Dash, Backdash, Sidestep, Attack,
    Block, CrouchBlock, Hit, CrouchHit, AirHit, Knockdown, GetUp, Thrown,
    StyleBreak, Clash, KO, Win, Taunt
};

enum class Phase { Intro, Fight, KO, RoundOver, MatchOver };

struct SimInput {
    int dirX = 0, dirY = 0;       // absolute screen directions (+x right, +y up)
    uint32_t held = 0, pressed = 0;
    const Move* requestMove = nullptr;   // AI shortcut: perform this move if legal (skips motion parsing)
};

struct FightEvent {
    enum Type { Hit, Block, Throw, Spark, Text, Projectile, Clash, StyleBreak, FlashCancel, KO, RoundStart, MoveName, Dust, Land, Tech } type;
    Vec3 pos;
    float strength = 1.0f;
    int player = 0;
    std::string text;
};

struct Projectile {
    Vec3 pos;
    float vx = 0;
    int owner = 0;
    int life = 0;
    HitWindow hit;
    Color color;
    bool alive = true;
    float spin = 0;
};

struct Fighter {
    const Character* ch = nullptr;
    int index = 0;
    Vec3 pos;                   // x along the fight line, y up, z lateral
    float vx = 0, vy = 0;       // knockback / jump velocity
    int facing = 1;
    FState state = FState::Idle;
    int timer = 0;              // frames in current state
    int stateLen = 0;           // length of timed states
    const Move* move = nullptr;
    int moveFrame = 0;
    bool hitDone[24] = {};
    bool moveConnected = false;
    bool armorUsed = false;
    int chainLevel = -1;        // highest normal level used in the current chain
    bool airAttackUsed = false;

    float hp = 1000, hpShown = 1000, hpRedShown = 1000;
    float hhg = 0;              // Heart Heat Gauge, 0..3000 (3 bars)
    float styleGauge = 1.0f;    // Stand gauge / Mode fuel
    bool styleOn = false;
    int styleLock = 0;          // frames until style can be activated again

    int hitstop = 0;
    int comboHits = 0;
    float comboDamage = 0;
    int comboShow = 0;          // frames to keep the combo counter visible
    bool inCombo = false;
    int invulStrikes = 0, invulThrows = 0;
    float sideTarget = 0;
    int roundsWon = 0;
    int proxGuard = 0;

    // input processing
    int dirHistory[32] = {};    // numpad notation relative to facing
    int histHead = 0;
    uint32_t pendingButtons = 0;
    int pendingAge = 0;
    int lastTapDir = 0, tapTimer = 0;
    int dashRequest = 0;        // +1 forward, -1 back (from double tap)
    bool neutralSeen = true;
    bool wantsBlock = false;
    bool hardKD = false;

    // presentation helpers
    float flash = 0;            // white flash on hit
    float standAttackBlend = 0; // 0..1 how far the Stand is "out front"
    int lastMoveNameTimer = 0;
    std::string lastMoveName;

    bool grounded() const { return pos.y <= 0.0001f; }
    bool attacking() const { return state == FState::Attack; }
    bool inHitstun() const { return state == FState::Hit || state == FState::CrouchHit || state == FState::AirHit || state == FState::Knockdown || state == FState::Thrown || state == FState::StyleBreak; }
    bool canAct() const { return state == FState::Idle || state == FState::Walk || state == FState::Crouch; }
    bool isDown() const { return state == FState::Knockdown || state == FState::GetUp; }
    float effectiveDamageMul() const { return (ch->style == StyleType::Mode && styleOn) ? 1.2f : 1.0f; }
    float effectiveSpeedMul() const { return (ch->style == StyleType::Mode && styleOn) ? 1.3f : 1.0f; }
};

struct ClashState {
    bool active = false;
    int timer = 0;
    int presses[2] = {0, 0};
};

class FightSim {
public:
    static constexpr float STAGE_HALF = 8.5f;
    static constexpr int ROUND_SECONDS = 99;

    void init(const Character* a, const Character* b, int roundsToWin = 2);
    void step(const SimInput& inA, const SimInput& inB);
    void startRound();

    Fighter& fighter(int i) { return f_[i]; }
    const Fighter& fighter(int i) const { return f_[i]; }
    Phase phase() const { return phase_; }
    int phaseTimer() const { return phaseTimer_; }
    int round() const { return round_; }
    int secondsLeft() const { return timerFrames_ / 60; }
    int winner() const { return winner_; }
    int matchWinner() const { return matchWinner_; }
    bool dramaticFinish() const { return dramatic_; }
    const ClashState& clash() const { return clash_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    std::vector<FightEvent>& events() { return events_; }
    float timeScale() const { return timeScale_; }
    float shake() const { return shake_; }
    int frame() const { return frame_; }
    int roundsToWin() const { return roundsToWin_; }

private:
    void updateInput(Fighter& f, const SimInput& in);
    void updateFighter(Fighter& f, Fighter& o, const SimInput& in);
    void updateFacing(Fighter& f, const Fighter& o);
    void updateMovement(Fighter& f, Fighter& o, const SimInput& in);
    bool tryStartMove(Fighter& f, Fighter& o, uint32_t group, bool fromCancel);
    bool tryFlashCancel(Fighter& f, uint32_t group);
    void beginMove(Fighter& f, const Move* m);
    void setState(Fighter& f, FState s, int len = 0);
    bool motionDetected(const Fighter& f, Motion m) const;
    void applyPhysics(Fighter& f);
    void resolveHits();
    void resolveProjectiles();
    bool checkHit(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 hitboxWorld, Vec3 size, bool fromProjectile);
    void applyHit(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 at, bool fromProjectile);
    void applyBlock(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 at);
    void pushboxes();
    void startClash();
    void updateClash(const SimInput& inA, const SimInput& inB);
    void checkRoundEnd();
    void emit(FightEvent::Type t, Vec3 pos, float strength = 1.0f, int player = 0, const std::string& text = "");

    Fighter f_[2];
    Phase phase_ = Phase::Intro;
    int phaseTimer_ = 0;
    int round_ = 1;
    int roundsToWin_ = 2;
    int timerFrames_ = ROUND_SECONDS * 60;
    int winner_ = -1, matchWinner_ = -1;
    bool dramatic_ = false;
    int frame_ = 0;
    float timeScale_ = 1.0f;
    float shake_ = 0;
    ClashState clash_;
    std::vector<Projectile> projectiles_;
    std::vector<FightEvent> events_;
    Rng rng_{12345};
};

} // namespace dc
