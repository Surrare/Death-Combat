#include "ui/canvas.h"

#include <algorithm>

namespace dc::ui {
namespace {

constexpr float safeAreaFraction = 0.05f;

glm::vec2 anchorFraction(Anchor anchor) noexcept {
    switch (anchor) {
    case Anchor::TopLeft: return {0.0f, 0.0f};
    case Anchor::Top: return {0.5f, 0.0f};
    case Anchor::TopRight: return {1.0f, 0.0f};
    case Anchor::Left: return {0.0f, 0.5f};
    case Anchor::Center: return {0.5f, 0.5f};
    case Anchor::Right: return {1.0f, 0.5f};
    case Anchor::BottomLeft: return {0.0f, 1.0f};
    case Anchor::Bottom: return {0.5f, 1.0f};
    case Anchor::BottomRight: return {1.0f, 1.0f};
    }
    return {0.5f, 0.5f};
}

}  // namespace

void Canvas::resize(int framebufferWidth, int framebufferHeight) noexcept {
    framebuffer_ = {static_cast<float>(std::max(framebufferWidth, 1)), static_cast<float>(std::max(framebufferHeight, 1))};
    scale_ = std::min(framebuffer_.x / canvasWidth, framebuffer_.y / canvasHeight);
}

glm::vec2 Canvas::toPixels(Anchor anchor, glm::vec2 offset) const noexcept {
    const glm::vec2 margin = framebuffer_ * safeAreaFraction;
    const glm::vec2 safeSize = framebuffer_ - margin * 2.0f;
    const glm::vec2 fraction = anchorFraction(anchor);
    return margin + safeSize * fraction + offset * scale_;
}

}  // namespace dc::ui
