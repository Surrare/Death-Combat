#include "gfx/textures.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dc {
namespace {

float smooth01(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float hash2(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFF) / (float)0x1000000;
}

// Value noise that repeats every `period` lattice cells, so the finished texture tiles without seams.
float tileNoise(float x, float y, int period, int seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    auto at = [&](int ix, int iy) {
        int px = ((ix % period) + period) % period, py = ((iy % period) + period) % period;
        return hash2(px, py, seed);
    };
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float a = at(xi, yi), b = at(xi + 1, yi), c = at(xi, yi + 1), d = at(xi + 1, yi + 1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

float fbm(float u, float v, int basePeriod, int seed) {
    float sum = 0, amp = 0.5, norm = 0;
    int period = basePeriod;
    for (int o = 0; o < 4; ++o) {
        sum += amp * tileNoise(u * period, v * period, period, seed + o * 17);
        norm += amp; amp *= 0.5f; period *= 2;
    }
    return sum / norm;
}

// height(u, v) in [0, 1] and a detail multiplier in [0, 1] for each surface kind
void sampleHeight(SurfaceTexture kind, float u, float v, float& h, float& detail) {
    switch (kind) {
    case SurfaceTexture::FloorTile: {
        // groove at the low edge of the cell (width 0.03), softly bevelled, plus fine grain and a few scratches
        float gx = smooth01(0.03f, 0.075f, u), gy = smooth01(0.03f, 0.075f, v);
        float bevel = std::min(gx, gy);
        float grain = fbm(u, v, 8, 3);
        float scratch = std::pow(std::max(0.0f, tileNoise(u * 3, v * 40, 3, 91) - 0.7f) * 1.6f, 2.0f);
        h = 0.55f * bevel + 0.22f * grain - 0.15f * scratch;
        detail = 0.86f + 0.14f * grain - 0.18f * scratch;
        break;
    }
    case SurfaceTexture::Brick: {
        // mortar at the low edge of the cell (0.05 across, 0.08 down); each brick has uneven chipped faces
        float mx = smooth01(0.05f, 0.095f, u), my = smooth01(0.08f, 0.14f, v);
        float body = std::min(mx, my);
        float chip = fbm(u, v, 5, 7);
        float pits = std::pow(std::max(0.0f, tileNoise(u * 24, v * 24, 24, 31) - 0.7f) * 3.3f, 1.5f);
        h = 0.5f * body + 0.38f * chip * body - 0.2f * pits * body;
        detail = (0.62f + 0.38f * body) * (0.82f + 0.18f * chip) - 0.12f * pits;
        break;
    }
    case SurfaceTexture::MetalPanel: {
        // groove at the low edge (0.04), rivets near the corners, long brushed streaks along u
        float gx = smooth01(0.04f, 0.075f, u), gy = smooth01(0.04f, 0.075f, v);
        float panel = std::min(gx, gy);
        float rivet = 0;
        const float rc[4][2] = {{0.18f, 0.18f}, {0.82f, 0.18f}, {0.18f, 0.82f}, {0.82f, 0.82f}};
        for (auto& c : rc) {
            float dx = u - c[0], dy = v - c[1];
            float r = std::sqrt(dx * dx + dy * dy);
            rivet = std::max(rivet, 1.0f - smooth01(0.012f, 0.032f, r));
        }
        float brushed = tileNoise(u * 2, v * 96, 2, 55) * 0.5f + tileNoise(u * 4, v * 160, 4, 56) * 0.5f;
        h = 0.5f * panel + 0.35f * rivet + 0.06f * brushed;
        detail = 0.8f + 0.2f * brushed - 0.18f * (1.0f - panel) + 0.08f * rivet;
        break;
    }
    }
    h = std::clamp(h, 0.0f, 1.0f);
    detail = std::clamp(detail, 0.0f, 1.0f);
}

} // namespace

TextureImage generateSurfaceTexture(SurfaceTexture kind, int size) {
    TextureImage img;
    img.size = size;
    img.rgba.resize((size_t)size * size * 4);
    std::vector<float> height((size_t)size * size), detail((size_t)size * size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            sampleHeight(kind, (x + 0.5f) / size, (y + 0.5f) / size, height[(size_t)y * size + x], detail[(size_t)y * size + x]);
    const float strength = kind == SurfaceTexture::Brick ? 5.5f : kind == SurfaceTexture::MetalPanel ? 4.0f : 2.4f;
    auto H = [&](int x, int y) { return height[(size_t)((y + size) % size) * size + ((x + size) % size)]; };
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // central differences with wrap-around: +u is +x, +v is +y (OpenGL texture rows start at v = 0)
            float dhdx = (H(x + 1, y) - H(x - 1, y)) * 0.5f * size / 64.0f;
            float dhdy = (H(x, y + 1) - H(x, y - 1)) * 0.5f * size / 64.0f;
            float nx = -dhdx * strength, ny = -dhdy * strength, nz = 1.0f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            uint8_t* p = &img.rgba[((size_t)y * size + x) * 4];
            p[0] = (uint8_t)std::lround((nx / len * 0.5f + 0.5f) * 255.0f);
            p[1] = (uint8_t)std::lround((ny / len * 0.5f + 0.5f) * 255.0f);
            p[2] = (uint8_t)std::lround((nz / len * 0.5f + 0.5f) * 255.0f);
            p[3] = (uint8_t)std::lround(detail[(size_t)y * size + x] * 255.0f);
        }
    }
    return img;
}

GLuint uploadTexture(const TextureImage& img, const char* debugName) {
    if (img.size <= 0 || img.rgba.size() != (size_t)img.size * img.size * 4) {
        std::fprintf(stderr, "[texture] %s has no pixel data\n", debugName);
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.size, img.size, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLenum err = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (err != GL_NO_ERROR) {
        std::fprintf(stderr, "[texture] %s upload failed (GL error 0x%04x)\n", debugName, err);
        glDeleteTextures(1, &tex);
        return 0;
    }
    std::printf("[texture] %s %dx%d RGBA8 with mipmaps\n", debugName, img.size, img.size);
    return tex;
}

} // namespace dc
