#include "presentation/fight_camera.h"

#include "presentation/units.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace dc::presentation {
namespace {

constexpr float fieldOfViewDegrees = 32.0f;
constexpr float minDistance = 6.0f;  // при 32° по вертикали в кадр входит ~3,4 м: пол и верх прыжка
constexpr float maxDistance = 10.0f;
constexpr float baseDistance = 5.0f;
constexpr float distancePerMeter = 0.9f;  // насколько отъезжаем на каждый метр между бойцами
constexpr float followSharpness = 8.0f;   // чем больше, тем быстрее камера догоняет цель
constexpr float cameraLift = 0.35f;
constexpr float targetHeight = 1.1f;  // точка, на которую смотрит камера, — чуть ниже груди бойца
constexpr float jumpFollow = 0.3f;    // камера чуть приподнимается за прыжком, но пол остаётся в кадре

// Экспоненциальное сглаживание, не зависящее от частоты кадров.
float approach(float current, float target, float frameSeconds) noexcept {
    return current + (target - current) * (1.0f - std::exp(-followSharpness * frameSeconds));
}

}  // namespace

void FightCamera::update(const sim::MatchState& state, float frameSeconds, float aspect) noexcept {
    const glm::vec2 a = toMeters(state.fighters[0].position);
    const glm::vec2 b = toMeters(state.fighters[1].position);
    const float separation = std::abs(b.x - a.x);

    const float targetX = (a.x + b.x) * 0.5f;
    const float targetY = targetHeight + jumpFollow * std::max(a.y, b.y);
    const float targetDistance = std::clamp(baseDistance + distancePerMeter * separation, minDistance, maxDistance);

    target_.x = approach(target_.x, targetX, frameSeconds);
    target_.y = approach(target_.y, targetY, frameSeconds);
    distance_ = approach(distance_, targetDistance, frameSeconds);
    aspect_ = aspect;
    // TODO(§7.3): тряска trauma² · шум, наезд на сильных ударах, треки камеры кинематик из glTF.
}

gfx::CameraData FightCamera::cameraData() const {
    gfx::CameraData camera;
    camera.position = target_ + glm::vec3{0.0f, cameraLift, distance_};
    camera.view = glm::lookAt(camera.position, target_, glm::vec3{0.0f, 1.0f, 0.0f});
    camera.projection = glm::perspective(glm::radians(fieldOfViewDegrees), aspect_, 0.1f, 200.0f);
    return camera;
}

}  // namespace dc::presentation
