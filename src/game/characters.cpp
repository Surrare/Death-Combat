#include "game/characters.h"
#include "game/input.h"

namespace dc {

namespace {

const uint32_t ATK[3] = {BTN_LIGHT, BTN_MEDIUM, BTN_HEAVY};
const char* STRENGTH[3] = {"L", "M", "H"};

HitWindow hit(int start, int end, float dmg, int hitstun, int blockstun, Vec3 pos, Vec3 size, float kx = 0.08f, float ky = 0.0f) {
    HitWindow h;
    h.start = start; h.end = end; h.damage = dmg; h.hitstun = hitstun; h.blockstun = blockstun;
    h.boxPos = pos; h.boxSize = size; h.knockX = kx; h.knockY = ky;
    return h;
}

Move normal(const char* name, uint32_t btn, int chain, Stance stance, AnimKind anim, int s, int a, int r, HitWindow h, StandUse su = StandUse::Any) {
    Move m; m.name = name; m.buttons = btn; m.chainLevel = chain; m.stance = stance; m.anim = anim;
    m.startup = s; m.active = a; m.recovery = r; m.stand = su;
    h.start = s; h.end = s + a - 1;
    m.hits.push_back(h);
    return m;
}

void addUniversal(Character& c) {
    using S = Stance;
    const bool standUser = c.style == StyleType::Stand;
    StandUse offOnly = standUser ? StandUse::Off : StandUse::Any;

    // ---- standing normals (style off)
    c.moves.push_back(normal("Jab", BTN_LIGHT, 0, S::Ground, AnimKind::Jab, 5, 3, 8,
        hit(0, 0, 40, 14, 8, {0.9f, 1.35f, 0}, {0.8f, 0.5f, 1.0f}, 0.06f), offOnly));
    c.moves.push_back(normal("Straight", BTN_MEDIUM, 1, S::Ground, AnimKind::StraightR, 8, 4, 14,
        hit(0, 0, 70, 18, 12, {1.0f, 1.3f, 0}, {1.0f, 0.6f, 1.0f}, 0.08f), offOnly));
    {
        Move m = normal("Roundhouse", BTN_HEAVY, 2, S::Ground, AnimKind::KickR, 12, 4, 20,
            hit(0, 0, 110, 24, 16, {1.1f, 1.2f, 0}, {1.2f, 0.9f, 1.0f}, 0.14f), offOnly);
        m.hits[0].hitstop = 8;
        c.moves.push_back(m);
    }
    // ---- standing normals (Stand on): longer reach, more damage, slower
    if (standUser) {
        Move l = normal("Stand Jab", BTN_LIGHT, 0, S::Ground, AnimKind::Jab, 7, 4, 10,
            hit(0, 0, 55, 16, 10, {1.5f, 1.3f, 0}, {1.3f, 0.6f, 1.0f}, 0.07f), StandUse::On);
        l.hits[0].standHit = true; l.standAttack = true;
        Move m = normal("Stand Straight", BTN_MEDIUM, 1, S::Ground, AnimKind::StraightL, 10, 5, 16,
            hit(0, 0, 85, 20, 13, {1.7f, 1.25f, 0}, {1.6f, 0.7f, 1.0f}, 0.09f), StandUse::On);
        m.hits[0].standHit = true; m.standAttack = true;
        Move h = normal("Stand Smash", BTN_HEAVY, 2, S::Ground, AnimKind::Launcher, 14, 6, 24,
            hit(0, 0, 130, 28, 18, {1.6f, 1.3f, 0}, {1.6f, 1.4f, 1.0f}, 0.10f, 0.22f), StandUse::On);
        h.hits[0].standHit = true; h.standAttack = true; h.hits[0].launcher = true; h.hits[0].hitstop = 9;
        c.moves.push_back(l); c.moves.push_back(m); c.moves.push_back(h);
    }
    // ---- crouching normals
    c.moves.push_back(normal("Low Jab", BTN_LIGHT, 0, S::Crouch, AnimKind::Jab, 5, 3, 8,
        hit(0, 0, 30, 13, 8, {0.85f, 0.75f, 0}, {0.8f, 0.5f, 1.0f}, 0.05f)));
    c.moves.back().hits[0].low = true;
    c.moves.push_back(normal("Low Kick", BTN_MEDIUM, 1, S::Crouch, AnimKind::LowKick, 8, 4, 14,
        hit(0, 0, 60, 18, 12, {1.1f, 0.35f, 0}, {1.2f, 0.5f, 1.0f}, 0.08f)));
    c.moves.back().hits[0].low = true;
    {
        Move m = normal("Sweep", BTN_HEAVY, 2, S::Crouch, AnimKind::Sweep, 11, 5, 22,
            hit(0, 0, 90, 30, 16, {1.2f, 0.3f, 0}, {1.4f, 0.5f, 1.0f}, 0.10f, 0.08f));
        m.hits[0].low = true; m.hits[0].knockdown = true; m.hits[0].hitstop = 8;
        c.moves.push_back(m);
    }
    // ---- air normals
    c.moves.push_back(normal("Air Jab", BTN_LIGHT, 0, S::Air, AnimKind::Jab, 5, 10, 4,
        hit(0, 0, 40, 14, 9, {0.8f, 1.1f, 0}, {0.9f, 0.7f, 1.0f}, 0.05f)));
    c.moves.back().hits[0].overhead = true;
    c.moves.push_back(normal("Air Kick", BTN_MEDIUM, 1, S::Air, AnimKind::KickL, 7, 12, 5,
        hit(0, 0, 65, 18, 12, {0.9f, 0.8f, 0}, {1.1f, 0.9f, 1.0f}, 0.08f)));
    c.moves.back().hits[0].overhead = true;
    c.moves.push_back(normal("Air Slam", BTN_HEAVY, 2, S::Air, AnimKind::Overhead, 9, 10, 6,
        hit(0, 0, 100, 24, 15, {0.9f, 0.7f, 0}, {1.2f, 1.1f, 1.0f}, 0.11f)));
    c.moves.back().hits[0].overhead = true; c.moves.back().hits[0].hitstop = 8;

    // ---- throw (L+M)
    {
        Move t; t.name = "Throw"; t.kind = MoveKind::Throw; t.buttons = BTN_LIGHT | BTN_MEDIUM;
        t.anim = AnimKind::Grab; t.startup = 4; t.active = 2; t.recovery = 22; t.commandThrow = true; t.unblockable = true;
        HitWindow h = hit(4, 5, 120, 40, 0, {0.7f, 1.0f, 0}, {0.9f, 1.6f, 0.8f}, 0.16f, 0.12f);
        h.knockdown = true; h.hardKnockdown = true; h.hitstop = 10;
        t.hits.push_back(h);
        t.specialCancel = false;
        c.moves.push_back(t);
    }
    // ---- taunt
    {
        Move t; t.name = "Taunt"; t.kind = MoveKind::Taunt; t.buttons = BTN_TAUNT; t.anim = AnimKind::Taunt;
        t.startup = 30; t.active = 1; t.recovery = 34; t.specialCancel = false;
        c.moves.push_back(t);
    }
}

Move special(const char* name, Motion motion, uint32_t btn, AnimKind anim, int s, int a, int r) {
    Move m; m.name = name; m.kind = MoveKind::Special; m.motion = motion; m.buttons = btn; m.anim = anim;
    m.startup = s; m.active = a; m.recovery = r;
    return m;
}

// Multi-hit forward rush (Stand barrage / kick flurry).
void addRush(Character& c, const char* name, Motion motion, int hits, float dmgPerHit, float speed, bool standAttack, int extraStartup = 0) {
    for (int k = 0; k < 3; ++k) {
        int interval = 4;
        int s = 9 + k * 2 + extraStartup;
        int n = hits + k;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], AnimKind::Rush, s, n * interval, 18 + k * 2);
        m.rush = true; m.standAttack = standAttack;
        m.moveSpeed = speed; m.moveStart = s; m.moveEnd = s + n * interval;
        for (int i = 0; i < n; ++i) {
            bool last = i == n - 1;
            HitWindow h = hit(s + i * interval, s + i * interval + interval - 1, dmgPerHit, last ? 26 : 12, last ? 14 : 8,
                              {standAttack ? 1.6f : 1.1f, 1.2f, 0}, {standAttack ? 1.6f : 1.2f, 1.0f, 1.0f}, last ? 0.16f : 0.015f, last ? 0.10f : 0.0f);
            h.standHit = standAttack; h.hitstop = last ? 8 : 3; h.chip = 0.08f;
            if (last) h.knockdown = true;
            m.hits.push_back(h);
        }
        c.moves.push_back(m);
    }
}

void addUppercut(Character& c, const char* name, Motion motion, AnimKind anim, float dmg) {
    for (int k = 0; k < 3; ++k) {
        int s = 5 + k;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], anim, s, 8, 26 + k * 4);
        m.invulStart = 0; m.invulEnd = s + 3 + k * 2;
        m.jumpVel = 0.12f + 0.03f * k; m.moveSpeed = 0.05f; m.moveStart = s; m.moveEnd = s + 8;
        HitWindow h = hit(s, s + 7, dmg + 20 * k, 30, 14, {0.8f, 1.4f, 0}, {1.0f, 1.8f, 1.0f}, 0.06f, 0.26f + 0.02f * k);
        h.launcher = true; h.hitstop = 9; h.chip = 0.08f;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

void addProjectile(Character& c, const char* name, Motion motion, float dmg, bool low, float baseSpeed, bool standAttack) {
    for (int k = 0; k < 3; ++k) {
        int s = 14 + k * 2;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], AnimKind::Cast, s, 2, 24 + k * 2);
        m.projectile = true; m.projectileSpeed = baseSpeed + 0.05f * k; m.standAttack = standAttack;
        HitWindow h = hit(s, s + 1, dmg + 10 * k, 22, 14, {0.9f, low ? 0.4f : 1.2f, 0}, {0.7f, 0.7f, 0.9f}, 0.12f, low ? 0.0f : 0.05f);
        h.low = low; h.chip = 0.1f; h.hitstop = 6; h.standHit = standAttack;
        if (low) h.knockdown = true;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

void addCommandThrow(Character& c, const char* name, Motion motion, float dmg) {
    for (int k = 0; k < 3; ++k) {
        int s = 6 + k * 2;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], AnimKind::Grab, s, 3, 28);
        m.commandThrow = true; m.unblockable = true; m.specialCancel = false;
        m.moveSpeed = 0.06f + 0.03f * k; m.moveStart = 0; m.moveEnd = s;
        HitWindow h = hit(s, s + 2, dmg + 25 * k, 50, 0, {0.8f, 1.0f, 0}, {1.1f, 1.7f, 0.8f}, 0.20f, 0.16f);
        h.knockdown = true; h.hardKnockdown = true; h.hitstop = 12;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

void addCharge(Character& c, const char* name, Motion motion, AnimKind anim, float dmg, bool armor) {
    for (int k = 0; k < 3; ++k) {
        int s = 12 + k * 3;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], anim, s, 6, 24);
        m.armor = armor; m.moveSpeed = 0.10f + 0.03f * k; m.moveStart = s - 4; m.moveEnd = s + 6;
        HitWindow h = hit(s, s + 5, dmg + 20 * k, 32, 18, {1.0f, 1.2f, 0}, {1.3f, 1.2f, 1.0f}, 0.22f, 0.12f);
        h.knockdown = true; h.hitstop = 10; h.chip = 0.1f;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

void addSlide(Character& c, const char* name, Motion motion, float dmg) {
    for (int k = 0; k < 3; ++k) {
        int s = 8 + k * 2;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], AnimKind::Slide, s, 10, 22);
        m.moveSpeed = 0.12f + 0.02f * k; m.moveStart = s - 2; m.moveEnd = s + 10;
        HitWindow h = hit(s, s + 9, dmg + 15 * k, 30, 14, {0.9f, 0.35f, 0}, {1.3f, 0.6f, 1.0f}, 0.10f, 0.10f);
        h.low = true; h.knockdown = true; h.hitstop = 8; h.chip = 0.06f;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

void addOverhead(Character& c, const char* name, Motion motion, AnimKind anim, float dmg) {
    for (int k = 0; k < 3; ++k) {
        int s = 16 + k * 2;
        Move m = special((std::string(name) + " " + STRENGTH[k]).c_str(), motion, ATK[k], anim, s, 5, 22);
        m.moveSpeed = 0.06f; m.moveStart = 4; m.moveEnd = s;
        HitWindow h = hit(s, s + 4, dmg + 20 * k, 34, 18, {1.0f, 1.0f, 0}, {1.2f, 1.6f, 1.0f}, 0.10f, 0.0f);
        h.overhead = true; h.knockdown = true; h.hitstop = 9; h.chip = 0.08f;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

// Heart Heat Attack: QCF + two attack buttons, 1 bar. Great Heat Attack: QCF + three buttons, 2 bars.
void addHeatAttacks(Character& c, const char* hhaName, const char* ghaName, bool standAttack, bool rushStyle) {
    {
        Move m = special(hhaName, Motion::QCF, BTN_LIGHT | BTN_MEDIUM, rushStyle ? AnimKind::Rush : AnimKind::Lariat, 10, 36, 30);
        m.kind = MoveKind::HHA; m.hhgCost = 1; m.invulStart = 0; m.invulEnd = 12; m.standAttack = standAttack; m.rush = rushStyle;
        m.moveSpeed = 0.07f; m.moveStart = 8; m.moveEnd = 40;
        int hits = rushStyle ? 8 : 3;
        int span = 36 / hits;
        for (int i = 0; i < hits; ++i) {
            bool last = i == hits - 1;
            HitWindow h = hit(10 + i * span, 10 + i * span + span - 1, 250.0f / hits, last ? 40 : 14, last ? 20 : 10,
                              {standAttack ? 1.5f : 1.1f, 1.2f, 0}, {standAttack ? 1.7f : 1.4f, 1.6f, 1.0f}, last ? 0.22f : 0.01f, last ? 0.18f : 0.0f);
            h.standHit = standAttack; h.hitstop = last ? 14 : 3; h.chip = 0.12f;
            if (last) { h.knockdown = true; h.hardKnockdown = true; }
            m.hits.push_back(h);
        }
        c.moves.push_back(m);
    }
    {
        Move m = special(ghaName, Motion::QCF, BTN_LIGHT | BTN_MEDIUM | BTN_HEAVY, AnimKind::Launcher, 14, 6, 40);
        m.kind = MoveKind::GHA; m.hhgCost = 2; m.invulStart = 0; m.invulEnd = 20; m.standAttack = standAttack;
        m.moveSpeed = 0.09f; m.moveStart = 4; m.moveEnd = 20;
        HitWindow h = hit(14, 19, 400, 60, 26, {standAttack ? 1.5f : 1.1f, 1.2f, 0}, {standAttack ? 1.9f : 1.5f, 2.0f, 1.0f}, 0.30f, 0.30f);
        h.standHit = standAttack; h.hitstop = 22; h.chip = 0.15f; h.knockdown = true; h.hardKnockdown = true; h.launcher = true;
        m.hits.push_back(h);
        c.moves.push_back(m);
    }
}

Character base(const char* name, const char* title, const char* styleName, StyleType style,
               Color body, Color accent, Color hair, Color styleColor, const char* lore) {
    Character c;
    c.name = name; c.title = title; c.styleName = styleName; c.style = style;
    c.bodyColor = body; c.accentColor = accent; c.hairColor = hair; c.styleColor = styleColor;
    c.lore = lore;
    return c;
}

std::vector<Character> buildRoster() {
    std::vector<Character> r;

    // 1. KAIN — crimson rush Stand
    {
        Character c = base("KAIN", "The Crimson Tempo", "BLOOD TEMPO", StyleType::Stand,
            Color(0.55f, 0.08f, 0.10f), Color(0.95f, 0.85f, 0.60f), Color(0.08f, 0.06f, 0.08f), Color(1.0f, 0.15f, 0.2f),
            "A brawler whose Stand beats in time with his heart. Close the distance and never stop punching.");
        c.hp = 1050;
        addUniversal(c);
        addRush(c, "Tempo Rush", Motion::QCF, 3, 28, 0.045f, true);
        addUppercut(c, "Crimson Riser", Motion::DP, AnimKind::Uppercut, 110);
        addOverhead(c, "Pulse Break", Motion::QCB, AnimKind::Stomp, 100);
        addHeatAttacks(c, "HUNDRED BEATS", "BLOOD FINALE", true, true);
        r.push_back(c);
    }
    // 2. VESPER — violet zoning Stand
    {
        Character c = base("VESPER", "The Night Glass", "NIGHT GLASS", StyleType::Stand,
            Color(0.18f, 0.10f, 0.32f), Color(0.80f, 0.75f, 0.95f), Color(0.85f, 0.85f, 0.95f), Color(0.65f, 0.30f, 1.0f),
            "A calm tactician who shatters her Stand into glass shards. Keep away and punish every approach.");
        c.hp = 950; c.walkSpeed = 0.052f;
        addUniversal(c);
        addProjectile(c, "Glass Shards", Motion::QCF, 70, false, 0.20f, true);
        addUppercut(c, "Rising Prism", Motion::DP, AnimKind::Launcher, 95);
        addSlide(c, "Mirror Step", Motion::QCB, 80);
        addHeatAttacks(c, "PRISM BARRAGE", "MIDNIGHT SHATTER", true, true);
        r.push_back(c);
    }
    // 3. ORSOLO — iron grappler Stand
    {
        Character c = base("ORSOLO", "The Iron Choir", "IRON CHOIR", StyleType::Stand,
            Color(0.30f, 0.30f, 0.34f), Color(0.80f, 0.60f, 0.25f), Color(0.45f, 0.40f, 0.35f), Color(0.75f, 0.80f, 0.90f),
            "A giant of a man whose Stand sings like a cathedral bell. Slow, armored and devastating up close.");
        c.hp = 1200; c.walkSpeed = 0.045f; c.backSpeed = 0.035f; c.height = 2.05f;
        addUniversal(c);
        addCharge(c, "Choir Lariat", Motion::QCF, AnimKind::Lariat, 120, true);
        addCommandThrow(c, "Iron Embrace", Motion::QCB, 180);
        addUppercut(c, "Bell Toll", Motion::DP, AnimKind::Headbutt, 120);
        addHeatAttacks(c, "REQUIEM SLAM", "CATHEDRAL FALL", true, false);
        r.push_back(c);
    }
    // 4. MIRA — pink rushdown Stand
    {
        Character c = base("MIRA", "The Silk Echo", "SILK ECHO", StyleType::Stand,
            Color(0.90f, 0.45f, 0.65f), Color(1.0f, 0.95f, 0.95f), Color(0.95f, 0.60f, 0.30f), Color(1.0f, 0.55f, 0.85f),
            "A dancer whose Stand weaves a thousand threads. Fast, slippery and relentless in the air.");
        c.hp = 900; c.walkSpeed = 0.064f; c.backSpeed = 0.05f; c.jumpVel = 0.215f;
        addUniversal(c);
        addRush(c, "Echo Needle", Motion::QCF, 4, 20, 0.05f, true, -2);
        addSlide(c, "Silk Veil", Motion::QCB, 85);
        addUppercut(c, "Thread Spiral", Motion::DP, AnimKind::Spin, 95);
        addHeatAttacks(c, "THOUSAND THREADS", "SILK REQUIEM", true, true);
        r.push_back(c);
    }
    // 5. KAZE — green speed Mode
    {
        Character c = base("KAZE", "The Gale", "GALE MODE", StyleType::Mode,
            Color(0.10f, 0.40f, 0.25f), Color(0.85f, 0.95f, 0.70f), Color(0.10f, 0.10f, 0.12f), Color(0.35f, 1.0f, 0.55f),
            "A martial artist without a Stand. Gale Mode spends his gauge for blinding speed.");
        c.hp = 950; c.walkSpeed = 0.060f; c.backSpeed = 0.048f; c.jumpVel = 0.21f;
        addUniversal(c);
        addRush(c, "Gale Kick", Motion::QCF, 3, 30, 0.07f, false);
        addSlide(c, "Tailwind", Motion::QCB, 90);
        addUppercut(c, "Updraft", Motion::DP, AnimKind::KneeStrike, 105);
        addHeatAttacks(c, "STORM LOTUS", "TYPHOON HEAVEN", false, true);
        r.push_back(c);
    }
    // 6. BRAM — ember power Mode
    {
        Character c = base("BRAM", "The Ember", "EMBER MODE", StyleType::Mode,
            Color(0.55f, 0.30f, 0.10f), Color(1.0f, 0.75f, 0.30f), Color(0.80f, 0.30f, 0.10f), Color(1.0f, 0.55f, 0.15f),
            "A blacksmith who burns his own stamina as fuel. Ember Mode turns every blow into a hammer strike.");
        c.hp = 1100; c.walkSpeed = 0.05f; c.backSpeed = 0.04f;
        addUniversal(c);
        addCharge(c, "Ember Fist", Motion::QCF, AnimKind::StraightR, 130, true);
        addProjectile(c, "Cinder Wave", Motion::QCB, 80, true, 0.14f, false);
        addUppercut(c, "Flare Rise", Motion::DP, AnimKind::Uppercut, 115);
        addHeatAttacks(c, "INFERNO HAMMER", "FORGE OF RUIN", false, false);
        r.push_back(c);
    }
    return r;
}

} // namespace

const std::vector<Character>& roster() {
    static std::vector<Character> r = buildRoster();
    return r;
}

} // namespace dc
