#include "platform/window.h"

#include "core/errors.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace dc::platform {
namespace {

void onGlfwError(int code, const char* description) {
    spdlog::error("GLFW {}: {}", code, description);
}

}  // namespace

void Window::GlfwWindowDeleter::operator()(GLFWwindow* window) const noexcept {
    glfwDestroyWindow(window);
}

Window::Window(const WindowDesc& desc) {
    glfwSetErrorCallback(onGlfwError);
    if (glfwInit() != GLFW_TRUE) {
        throw core::InitError{"Не удалось инициализировать GLFW"};
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, desc.debugContext ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    window_.reset(glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), nullptr, nullptr));
    if (!window_) {
        glfwTerminate();
        throw core::InitError{"Видеокарта или драйвер не поддерживают OpenGL 4.6 core. Обновите драйвер."};
    }

    glfwMakeContextCurrent(window_.get());
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        window_.reset();
        glfwTerminate();
        throw core::InitError{"GLAD не смог загрузить функции OpenGL 4.6"};
    }
    glfwSwapInterval(desc.vsync ? 1 : 0);

    spdlog::info("OpenGL {} | {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
}

Window::~Window() {
    window_.reset();
    glfwTerminate();
}

bool Window::shouldClose() const noexcept {
    return glfwWindowShouldClose(window_.get()) == GLFW_TRUE;
}

void Window::requestClose() noexcept {
    glfwSetWindowShouldClose(window_.get(), GLFW_TRUE);
}

void Window::pollEvents() noexcept {
    glfwPollEvents();
}

void Window::swapBuffers() noexcept {
    glfwSwapBuffers(window_.get());
}

FramebufferSize Window::framebufferSize() const noexcept {
    FramebufferSize size;
    glfwGetFramebufferSize(window_.get(), &size.width, &size.height);
    return size;
}

int Window::refreshRate() const noexcept {
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return mode != nullptr ? mode->refreshRate : 60;
}

}  // namespace dc::platform
