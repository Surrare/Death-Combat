#pragma once

#include "gfx/debug_draw.h"
#include "gfx/render_frame.h"

#include <optional>

namespace dc::gfx {

// Конвейер кадра (docs/ARCHITECTURE.md §8.4). Пока работают очистка экрана и отладочные линии;
// остальные проходы добавляются по дорожной карте (§14, трек A).
class Renderer {
public:
    Renderer();

    void render(const RenderFrame& frame, int width, int height);

private:
    std::optional<DebugDraw> debugDraw_;  // создаётся после включения GL debug output
};

}  // namespace dc::gfx
