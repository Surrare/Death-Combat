// Surface textures for the lit shader: tileable RGBA8 maps (RGB = tangent-space normal, A = detail / cavity).
// They are generated from height fields at startup, so the game still ships without any image files.
#pragma once
#include <glad/gl.h>
#include <vector>
#include <cstdint>

namespace dc {

struct TextureImage {
    int size = 0;
    std::vector<uint8_t> rgba;   // size * size * 4
};

enum class SurfaceTexture { FloorTile, Brick, MetalPanel };

// Builds one tileable map. The layout of the height field matches the procedural pattern in lit.frag:
// one texture period is one tile / brick / panel cell, with grooves at the low edge of the cell.
TextureImage generateSurfaceTexture(SurfaceTexture kind, int size = 256);

// Uploads RGBA8 with a full mip chain, repeat wrapping and trilinear filtering. Returns 0 on failure (and prints why).
GLuint uploadTexture(const TextureImage& img, const char* debugName);

} // namespace dc
