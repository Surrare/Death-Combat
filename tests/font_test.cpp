#include "core/font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

using namespace dc;

int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool ok, const char* reason) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", reason); }
    };
    FontAtlas font;
    check(font.build(), "embedded font atlas builds without system fonts or files");
    if (failures) return 1;
    check(font.pixels().size() == FontAtlas::WIDTH * FontAtlas::HEIGHT, "atlas size");
    check(std::any_of(font.pixels().begin(), font.pixels().end(), [](unsigned char v) { return v > 0 && v < 128; }), "distance field contains smooth edge samples");
    check(std::any_of(font.pixels().begin(), font.pixels().end(), [](unsigned char v) { return v > 128; }), "distance field contains glyph interiors");
    for (FontFace face : {FontFace::Body, FontFace::Display}) {
        for (float size : {12.6f, 15.3f, 27.0f, 72.0f}) {
            const auto title = font.layout("DEATH COMBAT", size, face);
            const auto twice = font.layout("DEATH COMBAT", size * 2, face);
            check(title.quads.size() == 11 && title.width > 0, "spaces advance without drawing");
            check(std::fabs(twice.width - title.width * 2) < 0.001f, "measurement scales with rendered text");
            check(font.layout("WWW", size, face).width > font.layout("III", size, face).width * 1.5f, "proportional character widths");
            const auto av = font.layout("AV", size, face);
            const float separate = font.layout("A", size, face).width + font.layout("V", size, face).width;
            check(av.width < separate, "real kerning for AV pair");
            check(font.layout("i", size, face).quads[0].v0 != font.layout("I", size, face).quads[0].v0
                || font.layout("i", size, face).quads[0].u0 != font.layout("I", size, face).quads[0].u0, "lowercase retains its own glyph");
            for (int code = 33; code <= 126; ++code) {
                const auto glyph = font.layout(std::string(1, static_cast<char>(code)), size, face);
                check(glyph.quads.size() == 1 && glyph.width > 0, "printable ASCII coverage");
                if (glyph.quads.empty()) continue;
                const auto& q = glyph.quads[0];
                check(q.u0 >= 0 && q.v0 >= 0 && q.u1 <= 1 && q.v1 <= 1 && q.u0 < q.u1 && q.v0 < q.v1, "glyph texture coordinates inside atlas");
                check(std::isfinite(q.x0) && q.x1 > q.x0 && q.y1 > q.y0, "valid glyph bounds");
            }
            const auto lines = font.layout("WW\nI", size, face);
            check(std::fabs(lines.width - font.layout("WW", size, face).width) < 0.001f, "multiline width is longest line");
            check(lines.quads.back().y0 > lines.quads.front().y0, "new line advances vertically");
            check(font.layout("", size, face).width == 0 && font.layout(" ", size, face).quads.empty(), "empty text and whitespace");
            check(font.layout("\xd0\x96", size, face).quads.size() == 1, "Cyrillic glyph is rendered");
            check(font.layout(u8"ӘәҒғҚқҢңӨөҰұҮүҺһІі", size, face).quads.size() == 18,
                "Kazakh-specific Cyrillic glyphs are rendered");
            check(font.layout("\xff", size, face).quads.size() == 1, "invalid UTF-8 is safe");
        }
    }
    check(font.layout("TEXT", 0, FontFace::Body).quads.empty(), "zero size");
    check(font.layout("TEXT", std::numeric_limits<float>::quiet_NaN(), FontFace::Body).quads.empty(), "invalid size");
    std::printf("Font layout: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
