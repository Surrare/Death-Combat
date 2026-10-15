#include <GLFW/glfw3.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <algorithm>

const int SCREEN_W = 1280;
const int SCREEN_H = 720;
const float GRAVITY = -2300.0f;
const float GROUND_Y = 120.0f;

// Все состояния бойца
enum State {
    IDLE,
    WALK,
    CROUCH,
    JUMP,
    ATTACK_LIGHT,
    ATTACK_HEAVY,
    ATTACK_UPPERCUT,
    ATTACK_SPECIAL,
    BLOCK,
    HITSTUN,
    DIZZY_FINISH,     // Состояние "FINISH HIM!"
    FATALITY_DEAD,    // Обезглавлен после Fatality
    DEAD
};

// Прямоугольник коллизий
struct Box {
    float x, y, w, h;
    bool intersects(const Box& o) const {
        return (x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y);
    }
};

// Частицы (кровь, искры, пламя факелов)
struct Particle {
    float x, y, vx, vy;
    float r, g, b, alpha;
    float size, life;
    bool leavesBlood;
};

// Следы крови на полу арены
struct BloodStain {
    float x, w;
};

// Энергетический снаряд (файербол)
struct Projectile {
    float x, y, vx;
    int owner; // 1 или 2
    float r, g, b;
    bool active;
};

// Отсеченная голова при Fatality
struct SeveredHead {
    bool active;
    float x, y, vx, vy, angle;
    float r, g, b;
};

std::vector<Particle> particles;
std::vector<BloodStain> bloodStains;
std::vector<Projectile> projectiles;
SeveredHead fatalityHead = { false, 0, 0, 0, 0, 0, 1, 1, 1 };

float screenShake = 0.0f;
float lightningFlash = 0.0f;

// --- ВСТРОЕННЫЙ РАСТРОВЫЙ ШРИФТ (3x5 битовая маска в восьмеричном коде) ---
unsigned short getGlyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    switch (c) {
        case 'A': return 025755; case 'B': return 065656; case 'C': return 034443;
        case 'D': return 065556; case 'E': return 074747; case 'F': return 074744;
        case 'G': return 034553; case 'H': return 055755; case 'I': return 072227;
        case 'J': return 011152; case 'K': return 055655; case 'L': return 044447;
        case 'M': return 057755; case 'N': return 057555; case 'O': return 025552;
        case 'P': return 065644; case 'Q': return 025571; case 'R': return 065655;
        case 'S': return 034216; case 'T': return 072222; case 'U': return 055557;
        case 'V': return 055552; case 'W': return 055775; case 'X': return 055255;
        case 'Y': return 055222; case 'Z': return 071247;
        case '0': return 075557; case '1': return 026227; case '2': return 071747;
        case '3': return 071717; case '4': return 055711; case '5': return 074717;
        case '6': return 074757; case '7': return 071111; case '8': return 075757;
        case '9': return 075717; case '!': return 022202; case ':': return 002020;
        case '-': return 000700; default:  return 000000;
    }
}

void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void drawText(const std::string& text, float x, float y, float pixelSize, float r, float g, float b, bool center = false) {
    float totalW = text.size() * (4.0f * pixelSize);
    float startX = center ? (x - totalW * 0.5f) : x;

    for (size_t i = 0; i < text.size(); ++i) {
        unsigned short glyph = getGlyph(text[i]);
        if (glyph == 0) continue;
        float cx = startX + i * 4.0f * pixelSize;
        for (int row = 0; row < 5; ++row) {
            int rowBits = (glyph >> ((4 - row) * 3)) & 7;
            for (int col = 0; col < 3; ++col) {
                if ((rowBits >> (2 - col)) & 1) {
                    float px = cx + col * pixelSize;
                    float py = y + (4 - row) * pixelSize;
                    // Тень текста
                    drawRect(px + pixelSize * 0.3f, py - pixelSize * 0.3f, pixelSize, pixelSize, 0.0f, 0.0f, 0.0f, 0.85f);
                    // Основной пиксель буквы
                    drawRect(px, py, pixelSize, pixelSize, r, g, b, 1.0f);
                }
            }
        }
    }
}

void drawLimb(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * (thickness * 0.5f);
    float ny = dx / len * (thickness * 0.5f);

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x1 + nx, y1 + ny);
    glVertex2f(x1 - nx, y1 - ny);
    glVertex2f(x2 - nx, y2 - ny);
    glVertex2f(x2 + nx, y2 + ny);
    glEnd();
}

void spawnParticles(float x, float y, int count, float r, float g, float b, float speedMul = 1.0f, bool isBlood = false) {
    for (int i = 0; i < count; ++i) {
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float speed = ((rand() % 380) + 60.0f) * speedMul;
        Particle p;
        p.x = x; p.y = y;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed + (isBlood ? 180.0f : 40.0f);
        p.r = r; p.g = g; p.b = b;
        p.alpha = 1.0f;
        p.size = (rand() % 6) + 4.0f;
        p.life = ((rand() % 55) + 25) / 100.0f;
        p.leavesBlood = isBlood;
        particles.push_back(p);
    }
}

// --- КЛАСС БОЙЦА ---
struct Fighter {
    int id;
    float x, y, vx, vy;
    int dir;
    float hp, maxHp;
    float energy; // От 0 до 100
    State state;
    float stateTimer;
    bool hasHitCurrentAttack;
    float r, g, b;
    bool isAI;
    int roundsWon;
    int comboCount;
    float comboTimer;

    Fighter(int playerID, float startX, int startDir, float cr, float cg, float cb, bool ai) {
        id = playerID;
        r = cr; g = cg; b = cb;
        isAI = ai;
        roundsWon = 0;
        reset(startX, startDir);
    }

    void reset(float startX, int startDir) {
        x = startX;
        y = GROUND_Y;
        vx = 0.0f; vy = 0.0f;
        dir = startDir;
        maxHp = 100.0f;
        hp = maxHp;
        energy = 50.0f;
        state = IDLE;
        stateTimer = 0.0f;
        hasHitCurrentAttack = false;
        comboCount = 0;
        comboTimer = 0.0f;
    }

    Box getHurtbox() const {
        if (state == CROUCH) return { x - 35.0f, y, 70.0f, 115.0f };
        return { x - 35.0f, y, 70.0f, 170.0f };
    }

    Box getAttackHitbox() const {
        if (state == ATTACK_LIGHT && stateTimer > 0.04f && stateTimer < 0.16f) {
            float hx = (dir == 1) ? x + 20.0f : x - 110.0f;
            return { hx, y + 95.0f, 90.0f, 35.0f };
        }
        if (state == ATTACK_HEAVY && stateTimer > 0.10f && stateTimer < 0.28f) {
            float hx = (dir == 1) ? x + 20.0f : x - 145.0f;
            return { hx, y + 65.0f, 125.0f, 50.0f };
        }
        if (state == ATTACK_UPPERCUT && stateTimer > 0.06f && stateTimer < 0.26f) {
            float hx = (dir == 1) ? x + 15.0f : x - 105.0f;
            return { hx, y + 80.0f, 90.0f, 110.0f };
        }
        return { 0, 0, 0, 0 };
    }

    void startAttack(State attackType) {
        if (state == IDLE || state == WALK || state == JUMP || state == CROUCH) {
            if (attackType == ATTACK_SPECIAL) {
                if (energy < 35.0f) return; // Не хватает энергии
                energy -= 35.0f;
            }
            state = attackType;
            stateTimer = 0.0f;
            hasHitCurrentAttack = false;
            if (y <= GROUND_Y + 1.0f) vx = 0.0f;
            if (attackType == ATTACK_UPPERCUT) {
                vy = 620.0f; // Подскок при апперкоте
                vx = dir * 180.0f;
            }
        }
    }

    void triggerFatality() {
        state = FATALITY_DEAD;
        stateTimer = 0.0f;
        hp = 0.0f;
        screenShake = 35.0f;
        lightningFlash = 1.0f;
        spawnParticles(x, y + 145.0f, 160, 0.9f, 0.0f, 0.0f, 2.0f, true);

        // Запускаем отсеченную голову в полет
        fatalityHead.active = true;
        fatalityHead.x = x;
        fatalityHead.y = y + 150.0f;
        fatalityHead.vx = -dir * 260.0f;
        fatalityHead.vy = 780.0f;
        fatalityHead.angle = 0.0f;
        fatalityHead.r = r; fatalityHead.g = g; fatalityHead.b = b;
    }

    void takeDamage(float dmg, float knockX, float knockY, float stun, Fighter& attacker, bool isHeavyOrSpecial) {
        if (state == DEAD || state == FATALITY_DEAD) return;

        // Если враг в состоянии "FINISH HIM!", тяжелый удар или спецприем делает FATALITY
        if (state == DIZZY_FINISH) {
            if (isHeavyOrSpecial) {
                triggerFatality();
            } else {
                state = DEAD;
                vy = 400.0f; vx = knockX;
                spawnParticles(x, y + 100.0f, 80, 0.85f, 0.0f, 0.0f, 1.5f, true);
            }
            return;
        }

        if (state == BLOCK) {
            hp -= dmg * 0.18f;
            energy = std::min(100.0f, energy + 6.0f);
            vx = knockX * 0.3f;
            spawnParticles(x + dir * 30.0f, y + 110.0f, 14, 0.3f, 0.85f, 1.0f, 0.7f, false);
            screenShake = 4.0f;
        } else {
            hp -= dmg;
            energy = std::min(100.0f, energy + 8.0f);
            attacker.energy = std::min(100.0f, attacker.energy + 14.0f);
            attacker.comboCount++;
            attacker.comboTimer = 1.8f;

            vx = knockX;
            vy = knockY;
            state = HITSTUN;
            stateTimer = stun;
            spawnParticles(x, y + 115.0f, 35, 0.9f, 0.05f, 0.05f, 1.2f, true);
            screenShake = isHeavyOrSpecial ? 15.0f : 7.0f;
        }

        if (hp <= 0.0f) {
            hp = 0.0f;
            // Если это решающий раунд для атакующего — включаем "FINISH HIM!"
            if (attacker.roundsWon == 1 && state != DIZZY_FINISH) {
                state = DIZZY_FINISH;
                stateTimer = 4.5f; // 4.5 секунды на добивание
                vx = 0.0f; vy = 0.0f;
                y = GROUND_Y;
            } else {
                state = DEAD;
                stateTimer = 0.0f;
                vy = 480.0f;
                vx = knockX * 1.4f;
                screenShake = 22.0f;
                spawnParticles(x, y + 90.0f, 100, 0.85f, 0.0f, 0.0f, 1.6f, true);
            }
        }
    }

    void update(float dt, Fighter& opp, bool roundActive) {
        stateTimer += dt;
        if (comboTimer > 0.0f) {
            comboTimer -= dt;
            if (comboTimer <= 0.0f) comboCount = 0;
        }

        // Кровотечение из шеи после Fatality
        if (state == FATALITY_DEAD && stateTimer < 3.5f) {
            if (rand() % 3 == 0) {
                spawnParticles(x, y + 130.0f, 4, 0.85f, 0.0f, 0.0f, 0.9f, true);
            }
        }

        // Таймер состояния "FINISH HIM!"
        if (state == DIZZY_FINISH) {
            stateTimer -= dt * 2.0f; // Уменьшаем оставшееся время
            if (stateTimer <= 0.0f) {
                state = DEAD;
                vy = 250.0f;
            }
            return;
        }

        if (state == IDLE || state == WALK || state == CROUCH || state == BLOCK) {
            dir = (opp.x >= x) ? 1 : -1;
        }

        // Выпуск огненного/плазменного снаряда в середине анимации ATTACK_SPECIAL
        if (state == ATTACK_SPECIAL && !hasHitCurrentAttack && stateTimer >= 0.14f) {
            hasHitCurrentAttack = true;
            Projectile proj;
            proj.x = x + dir * 55.0f;
            proj.y = y + 115.0f;
            proj.vx = dir * 780.0f;
            proj.owner = id;
            proj.r = (id == 1) ? 1.0f : 0.2f;
            proj.g = (id == 1) ? 0.35f : 0.85f;
            proj.b = (id == 1) ? 0.05f : 1.0f;
            proj.active = true;
            projectiles.push_back(proj);
            screenShake = 6.0f;
        }

        // Завершение анимаций ударов
        if (state == ATTACK_LIGHT && stateTimer >= 0.24f) state = (y > GROUND_Y) ? JUMP : IDLE;
        else if (state == ATTACK_HEAVY && stateTimer >= 0.44f) state = (y > GROUND_Y) ? JUMP : IDLE;
        else if (state == ATTACK_UPPERCUT && stateTimer >= 0.48f) state = (y > GROUND_Y) ? JUMP : IDLE;
        else if (state == ATTACK_SPECIAL && stateTimer >= 0.38f) state = (y > GROUND_Y) ? JUMP : IDLE;
        else if (state == HITSTUN) {
            stateTimer -= dt * 2.0f;
            if (stateTimer <= 0.0f) state = (y > GROUND_Y) ? JUMP : IDLE;
        }

        // Физика движения и гравитации
        x += vx * dt;
        y += vy * dt;

        if (y > GROUND_Y) {
            vy += GRAVITY * dt;
        } else {
            y = GROUND_Y;
            vy = 0.0f;
            if (state == JUMP) state = IDLE;
        }

        // Трение
        if (y <= GROUND_Y + 1.0f && (state == HITSTUN || state == DEAD || state == FATALITY_DEAD || state == BLOCK || state == IDLE || state == CROUCH)) {
            vx *= std::pow(0.0008f, dt);
        }

        // Стены арены
        if (x < 70.0f) x = 70.0f;
        if (x > SCREEN_W - 70.0f) x = SCREEN_W - 70.0f;

        // Расталкивание бойцов
        if (state != DEAD && state != FATALITY_DEAD && opp.state != DEAD && opp.state != FATALITY_DEAD) {
            if (getHurtbox().intersects(opp.getHurtbox())) {
                float push = 280.0f * dt;
                if (x < opp.x) { x -= push; opp.x += push; }
                else { x += push; opp.x -= push; }
            }
        }

        // Проверка попадания ближних атак
        if (roundActive && (state == ATTACK_LIGHT || state == ATTACK_HEAVY || state == ATTACK_UPPERCUT) && !hasHitCurrentAttack) {
            Box hit = getAttackHitbox();
            if (hit.w > 0 && hit.intersects(opp.getHurtbox())) {
                hasHitCurrentAttack = true;
                if (state == ATTACK_LIGHT) {
                    opp.takeDamage(6.5f, dir * 240.0f, 160.0f, 0.24f, *this, false);
                } else if (state == ATTACK_HEAVY) {
                    opp.takeDamage(14.0f, dir * 520.0f, 340.0f, 0.42f, *this, true);
                } else if (state == ATTACK_UPPERCUT) {
                    // Мощный подброс в воздух!
                    opp.takeDamage(17.0f, dir * 220.0f, 860.0f, 0.60f, *this, true);
                }
            }
        }
    }
};

// Отрисовка бойца
void renderFighter(const Fighter& f) {
    glPushMatrix();
    glTranslatef(f.x, f.y, 0.0f);

    if (f.state == DEAD) {
        glRotatef(f.dir * -90.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.0f, -30.0f, 0.0f);
    }

    float cr = (f.state == HITSTUN) ? 1.0f : f.r;
    float cg = (f.state == HITSTUN) ? 1.0f : f.g;
    float cb = (f.state == HITSTUN) ? 1.0f : f.b;

    float walkSwing = (f.state == WALK) ? std::sin(f.stateTimer * 15.0f) * 24.0f : 0.0f;
    float dizzySway = (f.state == DIZZY_FINISH) ? std::sin(glfwGetTime() * 8.0f) * 14.0f : 0.0f;
    float breathe = std::sin(f.stateTimer * 5.0f) * 3.0f;
    float crouchOffset = (f.state == CROUCH) ? -42.0f : 0.0f;

    // Тень
    if (f.state != DEAD) {
        drawRect(-36.0f, -6.0f, 72.0f, 12.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    }

    // Ноги
    float leftFootX = -18.0f + walkSwing;
    float rightFootX = 18.0f - walkSwing;
    float rightFootY = 0.0f;

    if (f.state == ATTACK_HEAVY && f.stateTimer > 0.08f && f.stateTimer < 0.32f) {
        rightFootX = f.dir * 98.0f;
        rightFootY = 88.0f;
    }

    drawLimb(-10.0f, 75.0f + crouchOffset, leftFootX, 0.0f, 16.0f, cr * 0.6f, cg * 0.6f, cb * 0.6f);
    drawLimb(10.0f, 75.0f + crouchOffset, rightFootX, rightFootY, 16.0f, cr * 0.8f, cg * 0.8f, cb * 0.8f);

    // Торс и жилет ниндзя
    drawLimb(dizzySway * 0.5f, 70.0f + crouchOffset, dizzySway, 135.0f + breathe + crouchOffset, 36.0f, cr, cg, cb);
    drawRect(-18.0f + dizzySway * 0.5f, 70.0f + crouchOffset, 36.0f, 12.0f, 0.12f, 0.12f, 0.12f);

    // Голова (отсутствует, если проведено FATALITY!)
    if (f.state != FATALITY_DEAD) {
        float hx = -16.0f + dizzySway;
        float hy = 140.0f + breathe + crouchOffset;
        drawRect(hx, hy, 32.0f, 32.0f, cr * 0.9f, cg * 0.9f, cb * 0.9f);
        drawRect(hx - 1.0f, hy + 14.0f, 34.0f, 10.0f, 0.1f, 0.1f, 0.1f);
        float eyeX = (f.dir == 1) ? (hx + 20.0f) : (hx + 4.0f);
        drawRect(eyeX, hy + 16.0f, 8.0f, 5.0f, 1.0f, 0.9f, 0.2f);
    }

    // Руки
    float punchExtend = 26.0f;
    float handY = 115.0f + breathe + crouchOffset;

    if (f.state == ATTACK_LIGHT && f.stateTimer > 0.03f && f.stateTimer < 0.18f) {
        punchExtend = 88.0f;
        handY = 122.0f + crouchOffset;
    } else if (f.state == ATTACK_UPPERCUT) {
        punchExtend = 48.0f;
        handY = 185.0f; // Рука взмывает вверх при апперкоте
    } else if (f.state == ATTACK_SPECIAL) {
        punchExtend = 70.0f;
        handY = 120.0f;
    } else if (f.state == BLOCK) {
        punchExtend = 20.0f;
        handY = 145.0f + crouchOffset;
    }

    drawLimb(dizzySway, 125.0f + breathe + crouchOffset, -f.dir * 20.0f + dizzySway, 100.0f + breathe + crouchOffset, 13.0f, cr * 0.7f, cg * 0.7f, cb * 0.7f);
    drawLimb(dizzySway, 125.0f + breathe + crouchOffset, f.dir * punchExtend + dizzySway, handY, 14.0f, cr, cg, cb);

    // Энергетический щит блока
    if (f.state == BLOCK) {
        drawRect(f.dir * 24.0f - 6.0f, 60.0f + crouchOffset, 12.0f, 100.0f, 0.2f, 0.85f, 1.0f, 0.55f);
    }

    glPopMatrix();

    // Вспышка хитбокса удара
    Box hit = f.getAttackHitbox();
    if (hit.w > 0) {
        drawRect(hit.x, hit.y, hit.w, hit.h, 1.0f, 0.85f, 0.2f, 0.30f);
    }
}

// Боевой ИИ второго игрока
void updateAI(Fighter& ai, const Fighter& target) {
    if (ai.state == DEAD || ai.state == FATALITY_DEAD || ai.state == DIZZY_FINISH || ai.state == HITSTUN || !ai.isAI) return;

    float dist = std::abs(target.x - ai.x);

    // Если враг в состоянии "FINISH HIM!" — ИИ идет добивать!
    if (target.state == DIZZY_FINISH) {
        if (dist > 110.0f) {
            ai.state = WALK;
            ai.vx = ai.dir * 380.0f;
        } else {
            ai.startAttack(ATTACK_UPPERCUT);
        }
        return;
    }

    // Реакция на атаку игрока
    if (target.state == ATTACK_LIGHT || target.state == ATTACK_HEAVY || target.state == ATTACK_UPPERCUT) {
        if (dist < 155.0f && (rand() % 100) < 40) {
            ai.state = BLOCK;
            ai.vx = 0.0f;
            return;
        }
    }

    if (dist > 260.0f && ai.energy >= 35.0f && (rand() % 100) < 4) {
        ai.startAttack(ATTACK_SPECIAL);
        return;
    }

    if (dist > 115.0f) {
        ai.state = WALK;
        ai.vx = ai.dir * 340.0f;
        if ((rand() % 180) < 2 && ai.y <= GROUND_Y + 1.0f) {
            ai.vy = 920.0f;
            ai.state = JUMP;
        }
    } else {
        ai.vx = 0.0f;
        int roll = rand() % 100;
        if (roll < 6) ai.startAttack(ATTACK_LIGHT);
        else if (roll < 10) ai.startAttack(ATTACK_HEAVY);
        else if (roll < 13) ai.startAttack(ATTACK_UPPERCUT);
        else if (roll < 18) ai.state = BLOCK;
        else if (ai.state == WALK || ai.state == BLOCK) ai.state = IDLE;
    }
}

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(SCREEN_W, SCREEN_H, "DEATH COMBAT: Ultimate Edition (OpenGL C++)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_W, 0, SCREEN_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Fighter p1(1, 350.0f, 1, 0.88f, 0.14f, 0.14f, false);
    Fighter p2(2, 930.0f, -1, 0.15f, 0.50f, 0.95f, true);

    int currentRound = 1;
    float roundTimer = 99.0f;
    float introBannerTimer = 2.2f;
    float roundOverTimer = 0.0f;
    bool roundScored = false;

    double lastTime = glfwGetTime();
    bool p1LP = false, p1HP = false, p1SP = false;
    bool p2LP = false, p2HP = false, p2SP = false;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        if (dt > 0.05f) dt = 0.05f;

        glfwPollEvents();

        // Полный перезапуск матча по клавише R
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            p1.roundsWon = 0; p2.roundsWon = 0;
            currentRound = 1;
            roundTimer = 99.0f;
            introBannerTimer = 2.2f;
            roundOverTimer = 0.0f;
            roundScored = false;
            p1.reset(350.0f, 1);
            p2.reset(930.0f, -1);
            particles.clear();
            bloodStains.clear();
            projectiles.clear();
            fatalityHead.active = false;
        }

        // Отключение ИИ при нажатии клавиш Игрока 2
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            p2.isAI = false;
        }

        bool roundActive = (introBannerTimer <= 0.5f) && (p1.state != DEAD && p1.state != FATALITY_DEAD && p2.state != DEAD && p2.state != FATALITY_DEAD);

        if (introBannerTimer > 0.0f) introBannerTimer -= dt;

        // Обратный отсчет таймера раунда
        if (roundActive && p1.state != DIZZY_FINISH && p2.state != DIZZY_FINISH) {
            roundTimer -= dt;
            if (roundTimer <= 0.0f) {
                roundTimer = 0.0f;
                if (p1.hp >= p2.hp) p2.state = DEAD;
                else p1.state = DEAD;
            }
        }

        // --- УПРАВЛЕНИЕ ИГРОКА 1 (WASD + F/G/E) ---
        bool p1CanControl = (p1.state != DEAD && p1.state != FATALITY_DEAD && p1.state != DIZZY_FINISH &&
                             p1.state != HITSTUN && p1.state != ATTACK_LIGHT && p1.state != ATTACK_HEAVY &&
                             p1.state != ATTACK_UPPERCUT && p1.state != ATTACK_SPECIAL && introBannerTimer <= 0.5f);

        if (p1CanControl) {
            bool crouchHeld = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && p1.y <= GROUND_Y + 1.0f);
            bool blockHeld = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && p1.y <= GROUND_Y + 1.0f);

            if (blockHeld) {
                p1.state = BLOCK;
                p1.vx = 0.0f;
            } else {
                if (crouchHeld) {
                    p1.state = CROUCH;
                    p1.vx = 0.0f;
                } else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    p1.vx = -390.0f;
                    if (p1.y <= GROUND_Y + 1.0f) p1.state = WALK;
                } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    p1.vx = 390.0f;
                    if (p1.y <= GROUND_Y + 1.0f) p1.state = WALK;
                } else {
                    p1.vx = 0.0f;
                    if (p1.y <= GROUND_Y + 1.0f) p1.state = IDLE;
                }

                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && p1.y <= GROUND_Y + 1.0f && !crouchHeld) {
                    p1.vy = 940.0f;
                    p1.state = JUMP;
                }

                if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
                    if (!p1LP) p1.startAttack(ATTACK_LIGHT);
                    p1LP = true;
                } else p1LP = false;

                if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
                    if (!p1HP) p1.startAttack(crouchHeld ? ATTACK_UPPERCUT : ATTACK_HEAVY);
                    p1HP = true;
                } else p1HP = false;

                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                    if (!p1SP) p1.startAttack(ATTACK_SPECIAL);
                    p1SP = true;
                } else p1SP = false;
            }
        }

        // --- УПРАВЛЕНИЕ ИГРОКА 2 (Стрелки + K/L/O + Правый Shift) ---
        bool p2CanControl = (p2.state != DEAD && p2.state != FATALITY_DEAD && p2.state != DIZZY_FINISH &&
                             p2.state != HITSTUN && p2.state != ATTACK_LIGHT && p2.state != ATTACK_HEAVY &&
                             p2.state != ATTACK_UPPERCUT && p2.state != ATTACK_SPECIAL && introBannerTimer <= 0.5f);

        if (p2.isAI && p2CanControl) {
            updateAI(p2, p1);
        } else if (p2CanControl) {
            bool crouchHeld = (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && p2.y <= GROUND_Y + 1.0f);
            bool blockHeld = (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS && p2.y <= GROUND_Y + 1.0f);

            if (blockHeld) {
                p2.state = BLOCK;
                p2.vx = 0.0f;
            } else {
                if (crouchHeld) {
                    p2.state = CROUCH;
                    p2.vx = 0.0f;
                } else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                    p2.vx = -390.0f;
                    if (p2.y <= GROUND_Y + 1.0f) p2.state = WALK;
                } else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                    p2.vx = 390.0f;
                    if (p2.y <= GROUND_Y + 1.0f) p2.state = WALK;
                } else {
                    p2.vx = 0.0f;
                    if (p2.y <= GROUND_Y + 1.0f) p2.state = IDLE;
                }

                if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && p2.y <= GROUND_Y + 1.0f && !crouchHeld) {
                    p2.vy = 940.0f;
                    p2.state = JUMP;
                }

                if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
                    if (!p2LP) p2.startAttack(ATTACK_LIGHT);
                    p2LP = true;
                } else p2LP = false;

                if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                    if (!p2HP) p2.startAttack(crouchHeld ? ATTACK_UPPERCUT : ATTACK_HEAVY);
                    p2HP = true;
                } else p2HP = false;

                if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
                    if (!p2SP) p2.startAttack(ATTACK_SPECIAL);
                    p2SP = true;
                } else p2SP = false;
            }
        }

        // Обновление бойцов
        p1.update(dt, p2, roundActive);
        p2.update(dt, p1, roundActive);

        // Обновление снарядов (файерболов)
        for (size_t i = 0; i < projectiles.size();) {
            Projectile& pr = projectiles[i];
            pr.x += pr.vx * dt;
            spawnParticles(pr.x, pr.y, 2, pr.r, pr.g, pr.b, 0.35f, false);

            Fighter& target = (pr.owner == 1) ? p2 : p1;
            Fighter& caster = (pr.owner == 1) ? p1 : p2;
            Box projBox = { pr.x - 22.0f, pr.y - 16.0f, 44.0f, 32.0f };

            if (target.state != DEAD && target.state != FATALITY_DEAD && projBox.intersects(target.getHurtbox())) {
                int knockDir = (pr.vx > 0) ? 1 : -1;
                target.takeDamage(18.0f, knockDir * 450.0f, 260.0f, 0.40f, caster, true);
                spawnParticles(pr.x, pr.y, 30, pr.r, pr.g, pr.b, 1.1f, false);
                pr.active = false;
            }

            if (pr.x < -50.0f || pr.x > SCREEN_W + 50.0f || !pr.active) {
                projectiles.erase(projectiles.begin() + i);
            } else {
                ++i;
            }
        }

        // Физика отрубленной головы при Fatality
        if (fatalityHead.active) {
            fatalityHead.x += fatalityHead.vx * dt;
            fatalityHead.y += fatalityHead.vy * dt;
            fatalityHead.vy += GRAVITY * dt;
            fatalityHead.angle += 360.0f * dt;
            if (fatalityHead.y <= GROUND_Y + 16.0f) {
                fatalityHead.y = GROUND_Y + 16.0f;
                fatalityHead.vy = -fatalityHead.vy * 0.45f;
                fatalityHead.vx *= 0.7f;
                if (std::abs(fatalityHead.vy) > 60.0f) {
                    bloodStains.push_back({ fatalityHead.x - 18.0f, 36.0f });
                }
            }
        }

        // Фоновые факелы арены
        if (rand() % 2 == 0) {
            spawnParticles(280.0f, 340.0f, 1, 1.0f, 0.45f, 0.05f, 0.25f, false);
            spawnParticles(1000.0f, 340.0f, 1, 1.0f, 0.45f, 0.05f, 0.25f, false);
        }

        // Обновление частиц и луж крови
        for (size_t i = 0; i < particles.size();) {
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
            if (particles[i].leavesBlood) {
                particles[i].vy += GRAVITY * 0.65f * dt;
            } else {
                particles[i].vy += 120.0f * dt; // Искры и огонь поднимаются
            }

            if (particles[i].y < GROUND_Y) {
                particles[i].y = GROUND_Y;
                particles[i].vx *= 0.4f;
                particles[i].vy = 0.0f;
                if (particles[i].leavesBlood && bloodStains.size() < 120 && (rand() % 4 == 0)) {
                    bloodStains.push_back({ particles[i].x - 8.0f, 16.0f });
                }
            }
            particles[i].life -= dt;
            particles[i].alpha = std::max(0.0f, particles[i].life * 2.0f);
            if (particles[i].life <= 0.0f) particles.erase(particles.begin() + i);
            else ++i;
        }

        // Проверка окончания раунда и перехода к следующему
        if (p1.state == DEAD || p1.state == FATALITY_DEAD || p2.state == DEAD || p2.state == FATALITY_DEAD) {
            if (!roundScored) {
                roundScored = true;
                if (p2.state == DEAD || p2.state == FATALITY_DEAD) p1.roundsWon++;
                else p2.roundsWon++;
                roundOverTimer = 3.5f;
            }
            roundOverTimer -= dt;
            if (roundOverTimer <= 0.0f && p1.roundsWon < 2 && p2.roundsWon < 2) {
                currentRound++;
                roundTimer = 99.0f;
                introBannerTimer = 2.2f;
                roundScored = false;
                p1.reset(350.0f, 1);
                p2.reset(930.0f, -1);
                projectiles.clear();
                fatalityHead.active = false;
            }
        }

        // --- РЕНДЕРИНГ СЦЕНЫ ---
        bool darkenSky = (p1.state == DIZZY_FINISH || p2.state == DIZZY_FINISH || p1.state == FATALITY_DEAD || p2.state == FATALITY_DEAD);
        glClearColor(darkenSky ? 0.02f : 0.08f, 0.03f, darkenSky ? 0.03f : 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glPushMatrix();
        if (screenShake > 0.0f) {
            float sx = (rand() % 100 / 50.0f - 1.0f) * screenShake;
            float sy = (rand() % 100 / 50.0f - 1.0f) * screenShake;
            glTranslatef(sx, sy, 0.0f);
            screenShake = std::max(0.0f, screenShake - 45.0f * dt);
        }

        // 1. Небо, кровавая луна и колонны храма
        drawRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, darkenSky ? 0.04f : 0.12f, 0.04f, darkenSky ? 0.05f : 0.15f);
        drawRect(540, 390, 200, 200, 0.75f, 0.06f, 0.10f, darkenSky ? 0.65f : 0.35f);
        for (int i = 100; i < SCREEN_W; i += 260) {
            drawRect(static_cast<float>(i), GROUND_Y, 52.0f, 460.0f, 0.06f, 0.03f, 0.08f);
        }

        // Подставки факелов
        drawRect(270.0f, 260.0f, 20.0f, 80.0f, 0.25f, 0.18f, 0.12f);
        drawRect(990.0f, 260.0f, 20.0f, 80.0f, 0.25f, 0.18f, 0.12f);

        // Вспышка молнии при FATALITY
        if (lightningFlash > 0.0f) {
            drawRect(0, 0, SCREEN_W, SCREEN_H, 1.0f, 0.95f, 0.95f, lightningFlash * 0.7f);
            Fighter& victim = (p1.state == FATALITY_DEAD) ? p1 : p2;
            drawLimb(victim.x + 40.0f, SCREEN_H, victim.x - 15.0f, victim.y + 280.0f, 14.0f, 0.9f, 0.95f, 1.0f);
            drawLimb(victim.x - 15.0f, victim.y + 280.0f, victim.x, victim.y + 120.0f, 10.0f, 0.9f, 0.95f, 1.0f);
            lightningFlash = std::max(0.0f, lightningFlash - 2.5f * dt);
        }

        // 2. Пол арены и лужи крови
        drawRect(0, 0, SCREEN_W, GROUND_Y, 0.15f, 0.13f, 0.17f);
        drawRect(0, GROUND_Y - 8.0f, SCREEN_W, 8.0f, 0.50f, 0.08f, 0.08f);
        for (const auto& b : bloodStains) {
            drawRect(b.x, GROUND_Y - 5.0f, b.w, 6.0f, 0.65f, 0.02f, 0.02f, 0.9f);
        }

        // 3. Бойцы
        renderFighter(p1);
        renderFighter(p2);

        // 4. Отрубленная голова (если было проведено Fatality)
        if (fatalityHead.active) {
            glPushMatrix();
            glTranslatef(fatalityHead.x, fatalityHead.y, 0.0f);
            glRotatef(fatalityHead.angle, 0.0f, 0.0f, 1.0f);
            drawRect(-16.0f, -16.0f, 32.0f, 32.0f, fatalityHead.r, fatalityHead.g, fatalityHead.b);
            drawRect(-17.0f, -2.0f, 34.0f, 10.0f, 0.1f, 0.1f, 0.1f);
            glPopMatrix();
        }

        // 5. Энергетические снаряды
        for (const auto& pr : projectiles) {
            drawRect(pr.x - 20.0f, pr.y - 14.0f, 40.0f, 28.0f, pr.r, pr.g, pr.b, 0.95f);
            drawRect(pr.x - 12.0f, pr.y - 8.0f, 24.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.85f);
        }

        // 6. Частицы
        for (const auto& p : particles) {
            drawRect(p.x - p.size * 0.5f, p.y - p.size * 0.5f, p.size, p.size, p.r, p.g, p.b, p.alpha);
        }

        glPopMatrix();

        // --- ИНТЕРФЕЙС (UI) ---
        float barW = 470.0f, barH = 28.0f, barY = SCREEN_H - 65.0f;

        // Имя и здоровье Игрока 1
        drawText("SCORPION-RED", 60.0f, barY + 34.0f, 3.5f, 1.0f, 0.85f, 0.2f);
        drawRect(56.0f, barY - 4.0f, barW + 8.0f, barH + 8.0f, 0.0f, 0.0f, 0.0f, 0.85f);
        drawRect(60.0f, barY, barW, barH, 0.3f, 0.05f, 0.05f);
        drawRect(60.0f, barY, barW * (p1.hp / p1.maxHp), barH, 0.9f, 0.15f, 0.15f);

        // Шкала энергии (Super Meter) Игрока 1
        drawRect(60.0f, barY - 18.0f, barW * 0.6f, 10.0f, 0.1f, 0.1f, 0.15f);
        drawRect(60.0f, barY - 18.0f, (barW * 0.6f) * (p1.energy / 100.0f), 10.0f, 1.0f, 0.65f, 0.0f);

        // Имя и здоровье Игрока 2
        float p2BarX = SCREEN_W - 60.0f - barW;
        drawText(p2.isAI ? "SUB-CYBER AI" : "SUB-CYBER P2", SCREEN_W - 245.0f, barY + 34.0f, 3.5f, 0.3f, 0.85f, 1.0f);
        drawRect(p2BarX - 4.0f, barY - 4.0f, barW + 8.0f, barH + 8.0f, 0.0f, 0.0f, 0.0f, 0.85f);
        drawRect(p2BarX, barY, barW, barH, 0.05f, 0.1f, 0.3f);
        float p2CurrW = barW * (p2.hp / p2.maxHp);
        drawRect(p2BarX + (barW - p2CurrW), barY, p2CurrW, barH, 0.2f, 0.55f, 0.95f);

        // Шкала энергии Игрока 2
        float p2EnW = (barW * 0.6f) * (p2.energy / 100.0f);
        drawRect(SCREEN_W - 60.0f - barW * 0.6f, barY - 18.0f, barW * 0.6f, 10.0f, 0.1f, 0.1f, 0.15f);
        drawRect(SCREEN_W - 60.0f - p2EnW, barY - 18.0f, p2EnW, 10.0f, 0.0f, 0.85f, 1.0f);

        // Индикаторы выигранных раундов (Золотые кристаллы)
        for (int i = 0; i < 2; ++i) {
            drawRect(60.0f + i * 26.0f, barY - 40.0f, 18.0f, 14.0f, (i < p1.roundsWon) ? 1.0f : 0.2f, (i < p1.roundsWon) ? 0.8f : 0.2f, 0.1f);
            drawRect(SCREEN_W - 78.0f - i * 26.0f, barY - 40.0f, 18.0f, 14.0f, (i < p2.roundsWon) ? 1.0f : 0.2f, (i < p2.roundsWon) ? 0.8f : 0.2f, 0.1f);
        }

        // Таймер раунда по центру
        drawRect(SCREEN_W * 0.5f - 42.0f, barY - 10.0f, 84.0f, 48.0f, 0.12f, 0.10f, 0.15f);
        std::string timeStr = std::to_string(static_cast<int>(std::ceil(roundTimer)));
        if (timeStr.size() == 1) timeStr = "0" + timeStr;
        drawText(timeStr, SCREEN_W * 0.5f, barY + 2.0f, 5.5f, 1.0f, 0.9f, 0.3f, true);
        drawText("DEATH COMBAT", SCREEN_W * 0.5f, SCREEN_H - 24.0f, 3.0f, 0.9f, 0.2f, 0.2f, true);

        // Счетчики КОМБО
        if (p1.comboCount >= 2) {
            drawText("KOMBO X" + std::to_string(p1.comboCount) + "!", 140.0f, SCREEN_H - 175.0f, 5.0f, 1.0f, 0.85f, 0.1f, true);
        }
        if (p2.comboCount >= 2) {
            drawText("KOMBO X" + std::to_string(p2.comboCount) + "!", SCREEN_W - 140.0f, SCREEN_H - 175.0f, 5.0f, 0.2f, 0.9f, 1.0f, true);
        }

        // Экранные анонсы ("ROUND X", "FIGHT!", "FINISH HIM!", "FATALITY!")
        if (introBannerTimer > 1.0f) {
            drawText("ROUND " + std::to_string(currentRound), SCREEN_W * 0.5f, SCREEN_H * 0.55f, 9.0f, 1.0f, 0.85f, 0.2f, true);
        } else if (introBannerTimer > 0.0f) {
            drawText("FIGHT!", SCREEN_W * 0.5f, SCREEN_H * 0.55f, 11.0f, 0.95f, 0.15f, 0.15f, true);
        }

        if (p1.state == DIZZY_FINISH || p2.state == DIZZY_FINISH) {
            drawText("FINISH HIM!", SCREEN_W * 0.5f, SCREEN_H * 0.60f, 10.0f, 0.95f, 0.1f, 0.1f, true);
        }

        if (p1.state == FATALITY_DEAD || p2.state == FATALITY_DEAD) {
            drawText("FATALITY!", SCREEN_W * 0.5f, SCREEN_H * 0.62f, 12.0f, 0.90f, 0.05f, 0.05f, true);
        }

        // Баннер чемпиона матча
        if ((p1.roundsWon >= 2 || p2.roundsWon >= 2) && (p1.state == DEAD || p1.state == FATALITY_DEAD || p2.state == DEAD || p2.state == FATALITY_DEAD)) {
            std::string winText = (p1.roundsWon >= 2) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
            drawText(winText, SCREEN_W * 0.5f, SCREEN_H * 0.46f, 8.0f, 1.0f, 0.9f, 0.2f, true);
            drawText("PRESS R TO RESTART MATCH", SCREEN_W * 0.5f, SCREEN_H * 0.36f, 4.0f, 0.8f, 0.8f, 0.8f, true);
        }

        // Подсказка управления внизу экрана
        drawText("P1: WASD  F:PUNCH  G:KICK  S+G:UPPERCUT  E:FIREBALL  LSHIFT:BLOCK", SCREEN_W * 0.5f, 18.0f, 2.3f, 0.65f, 0.65f, 0.7f, true);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}