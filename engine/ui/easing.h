#pragma once

namespace dc::ui {

// Функции плавности для твинов интерфейса (выезд HUD, пульс, вздрагивание полосы). t ∈ [0, 1].
[[nodiscard]] constexpr float easeOutCubic(float t) noexcept {
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

[[nodiscard]] constexpr float easeInOutQuad(float t) noexcept {
    return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

[[nodiscard]] constexpr float easeOutBack(float t) noexcept {
    constexpr float overshoot = 1.70158f;
    const float shifted = t - 1.0f;
    return 1.0f + (overshoot + 1.0f) * shifted * shifted * shifted + overshoot * shifted * shifted;
}

}  // namespace dc::ui
