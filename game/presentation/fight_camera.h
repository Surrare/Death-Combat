#pragma once

#include "gfx/render_frame.h"
#include "sim/match_state.h"

#include <glm/glm.hpp>

namespace dc::presentation {

// Камера боя (docs/ARCHITECTURE.md §7.3): смотрит на плоскость боя сбоку и держит обоих бойцов в кадре.
// Живёт в реальном времени кадра, а не в тиках: это представление, на исход боя она не влияет.
class FightCamera {
public:
    void update(const sim::MatchState& state, float frameSeconds, float aspect) noexcept;
    [[nodiscard]] gfx::CameraData cameraData() const;

private:
    glm::vec3 target_{0.0f, 1.1f, 0.0f};
    float distance_{7.0f};
    float aspect_{16.0f / 9.0f};
};

}  // namespace dc::presentation
