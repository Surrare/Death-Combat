#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace dc::platform {

struct WindowDesc {
    std::string title{"Death Combat"};
    int width{1600};
    int height{900};
    bool vsync{true};
    bool debugContext{false};  // включаем в Debug: GL сам сообщает об ошибках (docs/ARCHITECTURE.md §8.1)
};

struct FramebufferSize {
    int width{0};
    int height{0};
};

// Окно GLFW с контекстом OpenGL 4.6 core и загруженным GLAD. Один экземпляр на программу:
// конструктор инициализирует GLFW, деструктор его завершает.
class Window {
public:
    explicit Window(const WindowDesc& desc);  // InitError, если видеокарта не умеет OpenGL 4.6
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const noexcept;
    void requestClose() noexcept;
    void pollEvents() noexcept;
    void swapBuffers() noexcept;

    [[nodiscard]] FramebufferSize framebufferSize() const noexcept;
    [[nodiscard]] int refreshRate() const noexcept;  // Гц основного монитора, для снэппинга dt (§3)
    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_.get(); }

private:
    struct GlfwWindowDeleter {
        void operator()(GLFWwindow* window) const noexcept;
    };

    std::unique_ptr<GLFWwindow, GlfwWindowDeleter> window_;
};

}  // namespace dc::platform
