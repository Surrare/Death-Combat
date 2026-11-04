// Embedded TrueType fonts and a CPU-built signed-distance-field atlas.
#pragma once
#include <array>
#include <string_view>
#include <vector>

namespace dc {
enum class FontFace { Body, Display };

struct GlyphQuad {
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
};
struct TextLayout {
    std::vector<GlyphQuad> quads;
    float width = 0;
};

class FontAtlas {
public:
    bool build();
    TextLayout layout(std::string_view text, float capHeight, FontFace face) const;
    float lineHeight(float capHeight, FontFace face) const;
    const std::vector<unsigned char>& pixels() const { return pixels_; }
    static constexpr int WIDTH = 1024;
    static constexpr int HEIGHT = 1024;

private:
    static constexpr int FIRST = 32, COUNT = 95;
    static constexpr float CAP_HEIGHT = 48.0f;
    struct Glyph {
        float advance = 0;
        int x = 0, y = 0, width = 0, height = 0, offsetX = 0, offsetY = 0;
    };
    std::array<std::array<Glyph, COUNT>, 2> glyphs_{};
    std::array<std::array<float, COUNT * COUNT>, 2> kerning_{};
    std::array<float, 2> lineHeights_{};
    std::vector<unsigned char> pixels_;
    bool ready_ = false;
};
} // namespace dc
