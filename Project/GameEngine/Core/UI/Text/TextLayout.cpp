#include "pch.h"
#include "TextLayout.h"
#include "Utf8Decoder.h"
#include "Asset/Font/FontManager.h"

namespace GameEngine {
namespace {
struct LineRange {
   size_t begin = 0;
   size_t end = 0;
   float width = 0.0f;
};
}

TextLayoutResult TextLayout::Build(FontManager& fontManager, std::string_view text, const TextStyle& style) {
   TextLayoutResult result{};
   if (text.empty() || style.fontId.empty() || style.fontSize == 0 || !fontManager.HasFont(style.fontId)) {
      return result;
   }

   const auto codePoints = DecodeUtf8(text);
   if (codePoints.empty()) {
      return result;
   }

   const FontMetrics metrics = fontManager.GetMetrics(style.fontId, style.fontSize);
   const float lineHeight = metrics.lineHeight > 0.0f ? metrics.lineHeight : static_cast<float>(style.fontSize);
   const float lineAdvance = lineHeight * (std::max)(style.lineSpacing, 0.1f);
   float baseline = metrics.ascender > 0.0f ? metrics.ascender : static_cast<float>(style.fontSize);
   result.baseline = baseline;

   std::vector<LineRange> lines;
   size_t lineBegin = 0;
   float penX = 0.0f;
   float maximumLineWidth = 0.0f;
   uint32_t previousGlyphIndex = 0;

   const auto finishLine = [&]() {
      lines.push_back({ lineBegin, result.glyphs.size(), penX });
      maximumLineWidth = (std::max)(maximumLineWidth, penX);
      lineBegin = result.glyphs.size();
      penX = 0.0f;
      previousGlyphIndex = 0;
   };

   for (char32_t codePoint : codePoints) {
      if (codePoint == U'\r') {
         continue;
      }
      if (codePoint == U'\n') {
         finishLine();
         baseline += lineAdvance;
         continue;
      }

      const GlyphInfo* glyph = fontManager.GetOrCreateGlyph(style.fontId, style.fontSize, codePoint);
      if (!glyph) {
         continue;
      }

      float kerning = fontManager.GetKerning(
         style.fontId,
         previousGlyphIndex,
         glyph->glyphIndex,
         style.fontSize);
      if (style.maxWidth > 0.0f && penX > 0.0f && penX + kerning + glyph->advance > style.maxWidth) {
         finishLine();
         baseline += lineAdvance;
         kerning = 0.0f;
      }

      penX += kerning;
      result.glyphs.push_back({
         *glyph,
         { penX + glyph->bearing.x, baseline - glyph->bearing.y }
      });
      penX += glyph->advance;
      previousGlyphIndex = glyph->glyphIndex;
   }
   finishLine();

   const float layoutWidth = style.maxWidth > 0.0f ? style.maxWidth : maximumLineWidth;
   for (const LineRange& line : lines) {
      float offsetX = 0.0f;
      if (style.horizontalAlignment == TextHorizontalAlignment::Center) {
         offsetX = (layoutWidth - line.width) * 0.5f;
      } else if (style.horizontalAlignment == TextHorizontalAlignment::Right) {
         offsetX = layoutWidth - line.width;
      }

      for (size_t glyphIndex = line.begin; glyphIndex < line.end; ++glyphIndex) {
         result.glyphs[glyphIndex].position.x += offsetX;
      }
   }

   result.size.x = layoutWidth;
   result.size.y = lineHeight + static_cast<float>(lines.size() - 1u) * lineAdvance;
   return result;
}

} // namespace GameEngine
