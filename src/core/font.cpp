#include "core/font.h"
#include "font_body.h"
#include "font_display.h"
#include "font_cyrillic_body.h"
#include "font_cyrillic_display.h"
#include <algorithm>
#include <cmath>
#include <memory>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace dc {
namespace {
// Unsupported UTF-8 characters become one visible '?' instead of disappearing.
unsigned int nextCodepoint(std::string_view text, size_t& offset) {
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 0x80) return first;
    const int count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2
                    : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
    if (!count || offset + count > text.size()) return '?';
    unsigned int value = first & (0x7f >> count);
    for (int i = 0; i < count; ++i) {
        const auto c = static_cast<unsigned char>(text[offset + i]);
        if ((c & 0xc0) != 0x80) return '?';
        value = (value << 6) | (c & 0x3f);
    }
    offset += count;
    if ((count == 1 && value < 0x80) || (count == 2 && value < 0x800) || (count == 3 && value < 0x10000)
        || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return '?';
    return value;
}
}

unsigned int FontAtlas::codepointForIndex(int index) {
    if (index < ASCII_COUNT) return ASCII_FIRST + index;
    index -= ASCII_COUNT;
    if (index < 64) return 0x0410 + index;
    index -= 64;
    if (index == 0) return 0x0401;
    if (index == 1) return 0x0451;
    index -= 2;
    constexpr unsigned int kazakh[] = {
        0x0406, 0x0456, 0x0492, 0x0493, 0x049a, 0x049b, 0x04a2, 0x04a3, 0x04ae,
        0x04af, 0x04b0, 0x04b1, 0x04ba, 0x04bb, 0x04d8, 0x04d9, 0x04e8, 0x04e9
    };
    return kazakh[index];
}

int FontAtlas::indexForCodepoint(unsigned int codepoint) {
    if (codepoint >= ASCII_FIRST && codepoint < ASCII_FIRST + ASCII_COUNT)
        return static_cast<int>(codepoint - ASCII_FIRST);
    if (codepoint >= 0x0410 && codepoint <= 0x044f)
        return ASCII_COUNT + static_cast<int>(codepoint - 0x0410);
    if (codepoint == 0x0401) return ASCII_COUNT + 64;
    if (codepoint == 0x0451) return ASCII_COUNT + 65;
    constexpr unsigned int kazakh[] = {
        0x0406, 0x0456, 0x0492, 0x0493, 0x049a, 0x049b, 0x04a2, 0x04a3, 0x04ae,
        0x04af, 0x04b0, 0x04b1, 0x04ba, 0x04bb, 0x04d8, 0x04d9, 0x04e8, 0x04e9
    };
    for (int i = 0; i < static_cast<int>(std::size(kazakh)); ++i)
        if (codepoint == kazakh[i]) return ASCII_COUNT + 66 + i;
    return -1;
}

namespace {
struct SdfDeleter {
    void operator()(unsigned char* pixels) const { stbtt_FreeSDF(pixels, nullptr); }
};
}

bool FontAtlas::build() {
    ready_ = false;
    pixels_.assign(WIDTH * HEIGHT, 0);
    int penX = 1, penY = 1, rowHeight = 0;
    const unsigned char* sources[] = {font_data::font_body, font_data::font_display};
    const unsigned char* cyrillicSources[] = {font_data::font_cyrillic_body, font_data::font_cyrillic_display};
    for (int face = 0; face < 2; ++face) {
        stbtt_fontinfo info{};
        if (!stbtt_InitFont(&info, sources[face], 0)) return false;
        stbtt_fontinfo cyrillicInfo{};
        if (!stbtt_InitFont(&cyrillicInfo, cyrillicSources[face], 0)) return false;
        int x0, y0, x1, y1;
        if (!stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1) || y1 <= y0) return false;
        const float scale = CAP_HEIGHT / static_cast<float>(y1 - y0);
        if (!stbtt_GetCodepointBox(&cyrillicInfo, 'H', &x0, &y0, &x1, &y1) || y1 <= y0) return false;
        const float cyrillicScale = CAP_HEIGHT / static_cast<float>(y1 - y0);
        int ascent, descent, gap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
        lineHeights_[face] = static_cast<float>(ascent - descent + gap) * scale;
        for (int index = 0; index < COUNT; ++index) {
            const bool isCyrillic = index >= ASCII_COUNT;
            stbtt_fontinfo& glyphFont = isCyrillic ? cyrillicInfo : info;
            const float glyphScale = isCyrillic ? cyrillicScale : scale;
            const int codepoint = static_cast<int>(codepointForIndex(index));
            Glyph glyph;
            int advance, bearing;
            stbtt_GetCodepointHMetrics(&glyphFont, codepoint, &advance, &bearing);
            glyph.advance = static_cast<float>(advance) * glyphScale;
            std::unique_ptr<unsigned char, SdfDeleter> sdf(stbtt_GetCodepointSDF(&glyphFont, glyphScale, codepoint,
                5, 128, 32.0f, &glyph.width, &glyph.height, &glyph.offsetX, &glyph.offsetY));
            if (!sdf && codepoint != ' ') return false;
            if (sdf) {
                if (penX + glyph.width + 1 > WIDTH) {
                    penX = 1; penY += rowHeight + 1; rowHeight = 0;
                }
                if (penX + glyph.width + 1 > WIDTH || penY + glyph.height + 1 > HEIGHT) return false;
                glyph.x = penX; glyph.y = penY;
                for (int row = 0; row < glyph.height; ++row)
                    std::copy_n(sdf.get() + row * glyph.width, glyph.width,
                                pixels_.begin() + (penY + row) * WIDTH + penX);
                penX += glyph.width + 1;
                rowHeight = std::max(rowHeight, glyph.height);
            }
            glyphs_[face][index] = glyph;
            for (int next = 0; next < COUNT; ++next) {
                if (isCyrillic != (next >= ASCII_COUNT)) continue;
                const int nextCodepoint = static_cast<int>(codepointForIndex(next));
                kerning_[face][index * COUNT + next] =
                    static_cast<float>(stbtt_GetCodepointKernAdvance(&glyphFont, codepoint, nextCodepoint)) * glyphScale;
            }
        }
    }
    ready_ = true;
    return true;
}

float FontAtlas::lineHeight(float capHeight, FontFace face) const {
    return lineHeights_[static_cast<int>(face)] * capHeight / CAP_HEIGHT;
}

TextLayout FontAtlas::layout(std::string_view text, float capHeight, FontFace face) const {
    TextLayout result;
    if (!ready_ || !std::isfinite(capHeight) || capHeight <= 0) return result;
    result.quads.reserve(text.size());
    const int faceIndex = static_cast<int>(face);
    const float factor = capHeight / CAP_HEIGHT;
    const float tracking = capHeight * (face == FontFace::Display ? 0.025f : 0.012f);
    float x = 0, y = 0;
    int previous = -1;
    for (size_t offset = 0; offset < text.size();) {
        unsigned int codepoint = nextCodepoint(text, offset);
        if (codepoint == '\r') continue;
        if (codepoint == '\n') {
            result.width = std::max(result.width, x);
            x = 0; y += lineHeight(capHeight, face); previous = -1;
            continue;
        }
        if (codepoint == '\t') {
            x += glyphs_[faceIndex][0].advance * factor * 4; previous = -1;
            continue;
        }
        int index = indexForCodepoint(codepoint);
        if (index < 0) index = indexForCodepoint('?');
        const Glyph& glyph = glyphs_[faceIndex][index];
        if (previous >= 0) x += kerning_[faceIndex][previous * COUNT + index] * factor + tracking;
        if (glyph.width > 0 && glyph.height > 0) {
            const float gx = x + glyph.offsetX * factor;
            const float gy = y + capHeight + glyph.offsetY * factor;
            result.quads.push_back({gx, gy, gx + glyph.width * factor, gy + glyph.height * factor,
                static_cast<float>(glyph.x) / WIDTH, static_cast<float>(glyph.y) / HEIGHT,
                static_cast<float>(glyph.x + glyph.width) / WIDTH, static_cast<float>(glyph.y + glyph.height) / HEIGHT});
        }
        x += glyph.advance * factor;
        previous = index;
    }
    result.width = std::max(result.width, x);
    return result;
}
} // namespace dc
