#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "game/app.h"

static void glfwErrorCallback(int code, const char* desc) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, desc);
}

// Generates a crisp RGBA emblem icon for Death Combat (obsidian shield, gold crest, crossed crimson blades).
static std::vector<unsigned char> buildIconPixels(int size) {
    std::vector<unsigned char> px(size * size * 4, 0);
    const float half = (size - 1) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float u = (x - half) / half; // -1 .. 1
            float v = (y - half) / half; // -1 .. 1
            float ax = std::fabs(u);
            float ay = std::fabs(v);

            // Octagonal / heraldic shield boundary
            float shield = std::max({ax * 1.06f, ay * 1.04f, (ax + ay) * 0.76f});
            if (shield > 1.0f) continue;

            // Base obsidian slate gradient
            float r = 0.06f + 0.04f * (1.0f - v);
            float g = 0.07f + 0.04f * (1.0f - v);
            float b = 0.10f + 0.06f * (1.0f - v);
            float a = std::clamp((1.0f - shield) * size * 0.6f, 0.0f, 1.0f);

            // Inner crimson diamond glow
            float dia = ax + ay;
            if (dia < 0.78f) {
                float k = (0.78f - dia) / 0.78f;
                r += 0.48f * k * k;
                g += 0.06f * k * k;
                b += 0.09f * k * k;
            }

            // Outer champagne-gold architectural frame
            if (shield > 0.84f && shield <= 0.96f) {
                r = 0.90f; g = 0.75f; b = 0.40f;
            } else if (shield > 0.78f && shield <= 0.84f) {
                r = 0.18f; g = 0.16f; b = 0.14f;
            }

            // Crossed blades (hilts at bottom-left and bottom-right, tips at top-right and top-left)
            float d1 = std::fabs(u + v) * 0.7071f; // distance to bottom-left -> top-right diagonal (u = -v)
            float s1 = (u - v) * 0.7071f;          // position along blade 1 (- at bottom-left, + at top-right)
            float d2 = std::fabs(u - v) * 0.7071f; // distance to bottom-right -> top-left diagonal (u = v)
            float s2 = (-u - v) * 0.7071f;         // position along blade 2 (- at bottom-right, + at top-left)

            bool blade1 = (d1 < 0.082f * (1.0f - 0.35f * s1)) && (std::fabs(s1) < 0.74f);
            bool blade2 = (d2 < 0.082f * (1.0f - 0.35f * s2)) && (std::fabs(s2) < 0.74f);
            bool guard1 = (std::fabs(s1 + 0.36f) < 0.052f) && (d1 < 0.24f);
            bool guard2 = (std::fabs(s2 + 0.36f) < 0.052f) && (d2 < 0.24f);

            if (guard1 || guard2) {
                r = 0.94f; g = 0.78f; b = 0.38f;
            } else if (blade1 || blade2) {
                float edge = std::min(blade1 ? d1 : 1.0f, blade2 ? d2 : 1.0f);
                if (edge < 0.028f) {
                    r = 0.98f; g = 0.96f; b = 0.94f;
                } else {
                    r = 0.90f; g = 0.22f; b = 0.25f;
                }
            }

            // Center gold-crimson crest core
            float centerDist = std::sqrt(u * u + v * v);
            if (centerDist < 0.16f) {
                if (centerDist < 0.09f) { r = 0.95f; g = 0.20f; b = 0.22f; }
                else { r = 0.95f; g = 0.80f; b = 0.42f; }
            }

            int idx = (y * size + x) * 4;
            px[idx + 0] = (unsigned char)std::clamp(r * 255.0f, 0.0f, 255.0f);
            px[idx + 1] = (unsigned char)std::clamp(g * 255.0f, 0.0f, 255.0f);
            px[idx + 2] = (unsigned char)std::clamp(b * 255.0f, 0.0f, 255.0f);
            px[idx + 3] = (unsigned char)std::clamp(a * 255.0f, 0.0f, 255.0f);
        }
    }
    return px;
}

static void setWindowIcon(GLFWwindow* window) {
    std::vector<unsigned char> px64 = buildIconPixels(64);
    std::vector<unsigned char> px32 = buildIconPixels(32);
    GLFWimage images[2];
    images[0].width = 64; images[0].height = 64; images[0].pixels = px64.data();
    images[1].width = 32; images[1].height = 32; images[1].pixels = px32.data();
    glfwSetWindowIcon(window, 2, images);
}

// Options:
//   --demo-fight        AI vs AI fight (for testing)
//   --demo-hunt         jump straight into the hunt
//   --demo-select       character select screen
//   --demo-settings     settings screen
//   --demo-arenas       arena select (with --char / --arena)
//   --demo-grounds      hunting ground select (with --char / --arena)
//   --demo-split        versus match where both players back away, then return (split screen check)
//   --dev               enable Developer Mode overlay & hitboxes on startup
//   --shot <file> <s>   save a PNG screenshot after <s> seconds and quit
//   --seed <n>          deterministic seed for maze / effects
//   --char <n>          fighter index shown by --demo-select / --demo-hunt (0..5)
//   --arena <n>         arena for --demo-fight (0..3) or hunting ground for --demo-hunt (0..2)
//   --render-debug <mode> full, direct, no-bloom, no-outline, or no-shadows
int main(int argc, char** argv) {
    dc::AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--demo-fight") opts.demo = 1;
        else if (a == "--demo-hunt") opts.demo = 2;
        else if (a == "--demo-select") opts.demo = 3;
        else if (a == "--demo-settings") opts.demo = 4;
        else if (a == "--demo-arenas") opts.demo = 5;
        else if (a == "--demo-split") opts.demo = 6;
        else if (a == "--demo-grounds") opts.demo = 7;
        else if (a == "--dev") opts.devMode = true;
        else if (a == "--show-help") opts.showHelp = true;
        else if (a == "--shot" && i + 2 < argc) { opts.screenshotPath = argv[i + 1]; opts.screenshotTime = (float)std::atof(argv[i + 2]); i += 2; }
        else if (a == "--arena" && i + 1 < argc) { opts.arena = std::atoi(argv[i + 1]); i += 1; }
        else if (a == "--char" && i + 1 < argc) { opts.character = std::atoi(argv[i + 1]); i += 1; }
        else if (a == "--seed" && i + 1 < argc) { opts.seed = (uint64_t)std::atoll(argv[i + 1]); i += 1; }
        else if (a == "--render-debug" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "full") opts.renderDebugMode = dc::RenderDebugMode::Full;
            else if (mode == "direct") opts.renderDebugMode = dc::RenderDebugMode::Direct;
            else if (mode == "no-bloom") opts.renderDebugMode = dc::RenderDebugMode::NoBloom;
            else if (mode == "no-outline") opts.renderDebugMode = dc::RenderDebugMode::NoOutline;
            else if (mode == "no-shadows") opts.renderDebugMode = dc::RenderDebugMode::NoShadows;
            else {
                std::fprintf(stderr, "Unknown renderer debug mode: %s\n", mode.c_str());
                return 2;
            }
            opts.renderDebugModeSet = true;
        }
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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DEATH COMBAT — Ali Yerzhanuly, Daulet Ulgaisyn, Alikhan Kokey (Group: IT-2502)", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window (OpenGL 3.3 core required)\n");
        glfwTerminate();
        return 1;
    }
    setWindowIcon(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to load OpenGL functions\n");
        glfwTerminate();
        return 1;
    }
    std::printf("OpenGL %s | %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    glEnable(GL_MULTISAMPLE);

    bool initialized = false;
    {
        dc::App app;
        initialized = app.init(window, opts);
        if (initialized) {
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
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return initialized ? 0 : 1;
}
