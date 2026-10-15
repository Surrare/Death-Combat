#include "sim/state_hash.h"

namespace dc::sim {
namespace {

// FNV-1a, 64 бита.
class Hasher {
public:
    void add(std::uint64_t value) noexcept {
        for (int byte = 0; byte < 8; ++byte) {
            hash_ ^= (value >> (byte * 8)) & 0xFFu;
            hash_ *= 0x100000001b3ULL;
        }
    }
    void add(std::uint32_t value) noexcept { add(static_cast<std::uint64_t>(value)); }
    void add(std::int32_t value) noexcept { add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(value))); }
    void add(bool value) noexcept { add(static_cast<std::uint64_t>(value ? 1u : 0u)); }
    void add(const Vec2i& value) noexcept {
        add(value.x);
        add(value.y);
    }

    [[nodiscard]] std::uint64_t value() const noexcept { return hash_; }

private:
    std::uint64_t hash_{0xcbf29ce484222325ULL};
};

void addFighter(Hasher& hasher, const FighterState& fighter) noexcept {
    hasher.add(fighter.position);
    hasher.add(fighter.velocity);
    hasher.add(fighter.facing);
    hasher.add(static_cast<std::uint32_t>(fighter.action));
    hasher.add(static_cast<std::uint32_t>(fighter.move));
    hasher.add(fighter.actionFrame);
    hasher.add(fighter.stunFrames);
    hasher.add(fighter.health);
    hasher.add(fighter.offenseMeter);
    hasher.add(fighter.defenseMeter);
    hasher.add(fighter.comboHits);
    hasher.add(fighter.comboDamage);
    hasher.add(fighter.jugglePoints);
    hasher.add(fighter.roundsWon);
    hasher.add(fighter.moveConnected);
    hasher.add(fighter.fatalBlowUsed);
    for (int age = 0; age < InputBuffer::capacity; ++age) {
        const InputFrame frame = fighter.input.at(age);
        hasher.add(static_cast<std::uint32_t>(frame.direction));
        hasher.add(static_cast<std::uint32_t>(frame.buttons));
    }
}

}  // namespace

std::uint64_t hashState(const MatchState& state) noexcept {
    Hasher hasher;
    hasher.add(state.frame);
    for (const FighterState& fighter : state.fighters) {
        addFighter(hasher, fighter);
    }
    hasher.add(static_cast<std::uint64_t>(state.projectiles.size()));
    for (const Projectile& projectile : state.projectiles) {
        hasher.add(static_cast<std::uint32_t>(projectile.owner));
        hasher.add(projectile.position);
        hasher.add(projectile.velocity);
        hasher.add(projectile.hitbox.min);
        hasher.add(projectile.hitbox.max);
        hasher.add(projectile.facing);
        hasher.add(projectile.damage);
    }
    hasher.add(state.round.number);
    hasher.add(static_cast<std::uint32_t>(state.round.phase));
    hasher.add(state.round.phaseFrame);
    hasher.add(state.round.timerFrames);
    hasher.add(state.hitstop);

    core::Pcg32 rng = state.rng;  // следующее число однозначно определяется состоянием генератора
    hasher.add(rng.next());
    return hasher.value();
}

}  // namespace dc::sim
