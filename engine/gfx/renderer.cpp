#include "gfx/renderer.h"

#include "gfx/gl_debug.h"

#include <glad/gl.h>

namespace dc::gfx {

Renderer::Renderer() {
    installDebugOutput();  // раньше любых GL-ресурсов: драйвер сообщит о самой первой ошибке
    debugDraw_.emplace();
}

void Renderer::render(const RenderFrame& frame, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(frame.clearColor.r, frame.clearColor.g, frame.clearColor.b, frame.clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // TODO(§8.4, проходы 1–8): частицы → тени → маска крови → непрозрачное (HDR, MSAA ×4) → resolve
    //                          → прозрачное → bloom → композит (AgX, LUT) → UI.

    for (const DebugLine& line : frame.debugLines) {
        debugDraw_->addLine(line.from, line.to, line.color);
    }
    for (const DebugRect& rect : frame.debugRects) {
        debugDraw_->addRect(rect.min, rect.max, rect.color);
    }
    debugDraw_->flush(frame.camera.viewProjection());
}

}  // namespace dc::gfx
