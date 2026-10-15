#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace dc::gfx {

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};

    [[nodiscard]] glm::mat4 viewProjection() const { return projection * view; }
};

struct DebugLine {
    glm::vec3 from{0.0f};
    glm::vec3 to{0.0f};
    glm::vec4 color{1.0f};
};

// Прямоугольник в плоскости боя (z = 0), координаты в метрах: так рисуются хитбоксы и пушбоксы.
struct DebugRect {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
    glm::vec4 color{1.0f};
};

// Всё, что нужно рендеру для одного кадра. Заполняет его сцена (представление), а рендер только рисует:
// он не знает ни о бойцах, ни о раундах (docs/ARCHITECTURE.md §2).
struct RenderFrame {
    CameraData camera;
    glm::vec4 clearColor{0.05f, 0.03f, 0.07f, 1.0f};
    std::vector<DebugLine> debugLines;
    std::vector<DebugRect> debugRects;
    // TODO(§8.4): меши арены, скиннированные бойцы, источники света, эмиттеры частиц, маска крови, UI.
};

}  // namespace dc::gfx
