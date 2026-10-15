#pragma once

struct GLFWwindow;

namespace dc::gfx {

// Dear ImGui — только для отладочных окон (docs/ARCHITECTURE.md §10.4, §11). В релизном интерфейсе его нет.
class ImGuiLayer {
public:
    explicit ImGuiLayer(GLFWwindow* window);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void beginFrame();
    void endFrame();

    // ImGui забрал клавиатуру (например, фокус в поле ввода) — игре её в этот кадр не отдаём.
    [[nodiscard]] bool wantsKeyboard() const noexcept;
};

}  // namespace dc::gfx
