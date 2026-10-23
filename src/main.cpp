#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "game/app.h"

static void glfwErrorCallback(int code, const char* desc) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, desc);
}

// Options:
//   --demo-fight        AI vs AI fight (for testing)
//   --demo-hunt         jump straight into the hunt
//   --demo-select       character select screen
//   --shot <file> <s>   save a PNG screenshot after <s> seconds and quit
//   --seed <n>          deterministic seed for maze / effects
int main(int argc, char** argv) {
    dc::AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--demo-fight") opts.demo = 1;
        else if (a == "--demo-hunt") opts.demo = 2;
        else if (a == "--demo-select") opts.demo = 3;
        else if (a == "--shot" && i + 2 < argc) { opts.screenshotPath = argv[i + 1]; opts.screenshotTime = (float)std::atof(argv[i + 2]); i += 2; }
        else if (a == "--seed" && i + 1 < argc) { opts.seed = (uint64_t)std::atoll(argv[i + 1]); i += 1; }
    }
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DEATH COMBAT", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window (OpenGL 3.3 core required)\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to load OpenGL functions\n");
        glfwTerminate();
        return 1;
    }
    std::printf("OpenGL %s | %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    glEnable(GL_MULTISAMPLE);

    {
        dc::App app;
        if (!app.init(window, opts)) { glfwTerminate(); return 1; }
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window) && !app.wantsQuit()) {
            glfwPollEvents();
            double now = glfwGetTime();
            float dt = (float)(now - last);
            last = now;
            if (dt > 0.1f) dt = 0.1f;
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            if (w > 0 && h > 0) {
                app.frame(dt, w, h);
                glfwSwapBuffers(window);
            }
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
