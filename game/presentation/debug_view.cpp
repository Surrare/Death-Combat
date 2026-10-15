#include "presentation/debug_view.h"

#include "presentation/units.h"
#include "sim/boxes.h"

#include <array>
#include <cstddef>

namespace dc::presentation {
namespace {

const glm::vec4 floorColor{0.45f, 0.42f, 0.50f, 1.0f};
const glm::vec4 wallColor{0.60f, 0.15f, 0.15f, 1.0f};
const std::array<glm::vec4, 2> fighterColors{
    glm::vec4{0.95f, 0.30f, 0.25f, 1.0f},  // красный боец прототипа
    glm::vec4{0.30f, 0.65f, 1.00f, 1.0f},  // синий боец прототипа
};
constexpr float wallHeight = 4.0f;
constexpr float facingMarkerHeight = 1.5f;
constexpr float facingMarkerLength = 0.35f;

gfx::DebugRect toRect(const sim::Box& local, glm::vec2 position, std::int32_t facing, const glm::vec4& color) {
    const float minX = facing >= 0 ? toMeters(local.min.x) : -toMeters(local.max.x);
    const float maxX = facing >= 0 ? toMeters(local.max.x) : -toMeters(local.min.x);
    return gfx::DebugRect{{position.x + minX, position.y + toMeters(local.min.y)},
                          {position.x + maxX, position.y + toMeters(local.max.y)},
                          color};
}

}  // namespace

void addMatchDebugShapes(const sim::MatchState& previous, const sim::MatchState& current,
                         const sim::MatchData& data, float alpha, gfx::RenderFrame& frame) {
    const float wall = toMeters(data.rules.arenaHalfWidth);
    frame.debugLines.push_back({{-wall, 0.0f, 0.0f}, {wall, 0.0f, 0.0f}, floorColor});
    frame.debugLines.push_back({{-wall, 0.0f, 0.0f}, {-wall, wallHeight, 0.0f}, wallColor});
    frame.debugLines.push_back({{wall, 0.0f, 0.0f}, {wall, wallHeight, 0.0f}, wallColor});

    for (std::size_t i = 0; i < current.fighters.size(); ++i) {
        const sim::FighterState& before = previous.fighters[i];
        const sim::FighterState& now = current.fighters[i];
        const glm::vec2 position = glm::mix(toMeters(before.position), toMeters(now.position), alpha);

        const sim::Box& pushbox = sim::currentPushbox(now, data.characters[i]);
        frame.debugRects.push_back(toRect(pushbox, position, now.facing, fighterColors[i]));

        const glm::vec3 eye{position.x, position.y + facingMarkerHeight, 0.0f};
        const glm::vec3 look = eye + glm::vec3{facingMarkerLength * static_cast<float>(now.facing), 0.0f, 0.0f};
        frame.debugLines.push_back({eye, look, fighterColors[i]});
    }
    // TODO(§11): хёртбоксы (жёлтые) и активные хитбоксы (красные), когда в симуляции появятся приёмы.
}

}  // namespace dc::presentation
