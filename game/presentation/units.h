#pragma once

#include "sim/types.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace dc::presentation {

// Граница симуляции и картинки: целые миллиметры → метры float. Только в эту сторону (ADR-0003).
[[nodiscard]] inline float toMeters(std::int32_t millimeters) noexcept {
    return static_cast<float>(millimeters) * 0.001f;
}

[[nodiscard]] inline glm::vec2 toMeters(const sim::Vec2i& position) noexcept {
    return {toMeters(position.x), toMeters(position.y)};
}

}  // namespace dc::presentation
