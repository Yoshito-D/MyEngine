#pragma once

#include "Utility/VectorMath.h"
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {

/// @brief 画面上でUIテキストを配置する基準位置
enum class UIAnchor {
   TopLeft,
   TopCenter,
   TopRight,
   MiddleLeft,
   MiddleCenter,
   MiddleRight,
   BottomLeft,
   BottomCenter,
   BottomRight
};

/// @brief 複数行テキストの水平方向の揃え方
enum class TextHorizontalAlignment {
   Left,
   Center,
   Right
};

/// @brief グリフアトラスが保持する距離・カバレッジ情報の形式
enum class TextAtlasType : uint32_t {
   Bitmap,
   Msdf,
   Mtsdf
};

/// @brief UIテキストの表示設定
struct TextStyle {
   std::string fontId;
   uint32_t fontSize = 32;
   Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
   UIAnchor screenAnchor = UIAnchor::TopLeft;
   Vector2 pivot = { 0.0f, 0.0f };
   TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Left;
   float maxWidth = 0.0f;
   float lineSpacing = 1.0f;
   int32_t sortingOrder = 0;
};

/// @brief FreeTypeから取り出した1グリフ分のCPUビットマップ
struct RasterizedGlyph {
   std::vector<uint8_t> pixels;
   uint32_t width = 0;
   uint32_t height = 0;
   int32_t bitmapLeft = 0;
   int32_t bitmapTop = 0;
   float advanceX = 0.0f;
   uint32_t glyphIndex = 0;
};

/// @brief グリフアトラスに登録された文字の描画情報
struct GlyphInfo {
   Vector2 uvMin = { 0.0f, 0.0f };
   Vector2 uvMax = { 0.0f, 0.0f };
   Vector2 bitmapSize = { 0.0f, 0.0f };
   Vector2 bearing = { 0.0f, 0.0f };
   float advance = 0.0f;
   uint32_t glyphIndex = 0;
   uint32_t atlasPage = 0;
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv = {};
   Vector2 atlasSize = { 1.0f, 1.0f };
   float distanceRange = 0.0f;
   TextAtlasType atlasType = TextAtlasType::Bitmap;
};

/// @brief フォントサイズから得られる行レイアウト用メトリクス
struct FontMetrics {
   float ascender = 0.0f;
   float descender = 0.0f;
   float lineHeight = 0.0f;
};

/// @brief レイアウト済みグリフのローカル座標
struct GlyphPlacement {
   GlyphInfo glyph;
   Vector2 position = { 0.0f, 0.0f };
};

/// @brief テキスト全体のレイアウト結果
struct TextLayoutResult {
   std::vector<GlyphPlacement> glyphs;
   Vector2 size = { 0.0f, 0.0f };
   float baseline = 0.0f;
};

} // namespace GameEngine
