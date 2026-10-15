#pragma once

#include <glm/glm.hpp>

namespace dc::ui {

// Интерфейс верстается на виртуальном холсте 1920×1080 с якорями и безопасной зоной 5 %
// (docs/ARCHITECTURE.md §10.4) и масштабируется под любое окно: 16:9, 16:10, 21:9.
inline constexpr float canvasWidth = 1920.0f;
inline constexpr float canvasHeight = 1080.0f;

enum class Anchor { TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

class Canvas {
public:
    void resize(int framebufferWidth, int framebufferHeight) noexcept;

    // Смещение от якоря в единицах холста → пиксели экрана (y вниз, как в UI).
    [[nodiscard]] glm::vec2 toPixels(Anchor anchor, glm::vec2 offset) const noexcept;
    [[nodiscard]] float scale() const noexcept { return scale_; }

private:
    glm::vec2 framebuffer_{canvasWidth, canvasHeight};
    float scale_{1.0f};
};

// TODO(§10.4): UiRenderer — батчер спрайтов и MSDF-текст (латиница + кириллица), премультиплицированная альфа.

}  // namespace dc::ui
