#pragma once

#include "GameEngine/Math/VectorMath.h"
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
   std::string fontId;                        ///< 文字描画に使うフォントの登録ID。
   uint32_t fontSize = 32;                    ///< 文字サイズの指定値（ピクセル）。
   Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};  ///< 文字描画のRGBA色。
   UIAnchor screenAnchor = UIAnchor::TopLeft; ///< 画面上でUI配置の基準にするアンカー。
   Vector2 pivot = {0.0f, 0.0f};              ///< 要素の幅・高さに対する変換の基準点（0から1）。
   /// @brief 各行を左・中央・右のどこへ揃えるかの設定。
   TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Left;
   float maxWidth = 0.0f;    ///< 自動折り返しの上限幅。0以下では制限しない。
   float lineSpacing = 1.0f; ///< 基準行高に掛ける行送り倍率。
   int32_t sortingOrder = 0; ///< UI要素を前後に並べる描画順。
};

/// @brief グリフアトラスに登録された文字の描画情報
struct GlyphInfo {
   Vector2 uvMin = {0.0f, 0.0f};                    ///< グリフ画像の左上を参照する正規化UV。
   Vector2 uvMax = {0.0f, 0.0f};                    ///< グリフ画像の右下を参照する正規化UV。
   Vector2 bitmapSize = {0.0f, 0.0f};               ///< 表示サイズへ換算したグリフ画像の幅・高さ。
   Vector2 bearing = {0.0f, 0.0f};                  ///< 文字原点・ベースラインからグリフ画像までのオフセット。
   float advance = 0.0f;                            ///< 指定した文字サイズにおける次の文字原点までの送り幅。
   uint32_t glyphIndex = 0;                         ///< フォント内で文字を識別するグリフ番号。
   uint32_t atlasPage = 0;                          ///< グリフ画像を格納したアトラスページ番号。
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv = {};       ///< フォントアトラスをシェーダーから読むSRVのGPUハンドル。
   Vector2 atlasSize = {1.0f, 1.0f};                ///< アトラス画像の幅・高さ（ピクセル）。
   float distanceRange = 0.0f;                      ///< アトラスに符号化した符号付き距離の範囲。
   TextAtlasType atlasType = TextAtlasType::Bitmap; ///< MSDFまたはビットマップなど文字画像の解釈方式。
};

/// @brief フォントサイズから得られる行レイアウト用メトリクス
struct FontMetrics {
   float ascender = 0.0f;   ///< ベースラインから上端までの距離。
   float descender = 0.0f;  ///< ベースラインから下端までの距離。
   float lineHeight = 0.0f; ///< フォントの基準行高。
};

/// @brief レイアウト済みグリフのローカル座標
struct GlyphPlacement {
   GlyphInfo glyph;                 ///< 文字の画像・配置・送り量をまとめたグリフ情報。
   Vector2 position = {0.0f, 0.0f}; ///< レイアウト原点を基準としたグリフ画像の左上位置。
};

/// @brief テキスト全体のレイアウト結果
struct TextLayoutResult {
   std::vector<GlyphPlacement> glyphs; ///< レイアウト順に並べた文字ごとのグリフと配置位置。
   Vector2 size = {0.0f, 0.0f};        ///< 行送りと折り返しを含む文章全体の幅・高さ（ピクセル）。
   float baseline = 0.0f;              ///< 最初の行のベースライン位置。
};

} // namespace GameEngine
