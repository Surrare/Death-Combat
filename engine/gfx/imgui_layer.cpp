#include "gfx/imgui_layer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <filesystem>

namespace dc::gfx {

ImGuiLayer::ImGuiLayer(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Стандартный шрифт ImGui знает только латиницу; для русских подписей берём системный Segoe UI.
    const std::filesystem::path font{"C:/Windows/Fonts/segoeui.ttf"};
    if (std::filesystem::exists(font)) {
        io.Fonts->AddFontFromFileTTF(font.string().c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    }

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiLayer::wantsKeyboard() const noexcept {
    return ImGui::GetIO().WantCaptureKeyboard;
}

}  // namespace dc::gfx
