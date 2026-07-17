#pragma once

#include "Core/UI/Text/TextTypes.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include <filesystem>
#include <vector>

namespace GameEngine {

/// @brief フォントファイルとFreeTypeフェイスの寿命を管理する
class FontFace {
public:
   /// @brief 未読み込みのフォントフェイスを作成する
   FontFace() = default;

   /// @brief 保持するFreeTypeフェイスを解放する
   ~FontFace();

   FontFace(const FontFace&) = delete;
   FontFace& operator=(const FontFace&) = delete;

   /// @brief フォントファイルをメモリへ読み込みFreeTypeフェイスを作成する
   /// @param library 初期化済みFreeTypeライブラリ
   /// @param filePath TTFまたはOTFファイルのパス
   /// @return 読み込みに成功した場合はtrue
   bool Load(FT_Library library, const std::filesystem::path& filePath);

   /// @brief 指定文字を8bitグレースケール画像へラスタライズする
   /// @param codePoint Unicodeコードポイント
   /// @param pixelSize ピクセル単位のフォントサイズ
   /// @param output ラスタライズ結果
   /// @return 読み込みに成功した場合はtrue
   bool RasterizeGlyph(char32_t codePoint, uint32_t pixelSize, RasterizedGlyph& output);

   /// @brief 指定サイズの行メトリクスを取得する
   /// @param pixelSize ピクセル単位のフォントサイズ
   /// @return 行レイアウト用メトリクス
   FontMetrics GetMetrics(uint32_t pixelSize);

   /// @brief 連続する2グリフ間のカーニング量を取得する
   /// @param leftGlyph 左側グリフインデックス
   /// @param rightGlyph 右側グリフインデックス
   /// @param pixelSize ピクセル単位のフォントサイズ
   /// @return 水平方向のカーニング量（ピクセル）
   float GetKerning(uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize);

   /// @brief フォントが指定コードポイントを持つか確認する
   /// @param codePoint Unicodeコードポイント
   /// @return 対応グリフが存在する場合はtrue
   bool HasGlyph(char32_t codePoint) const;

private:
   bool SetPixelSize(uint32_t pixelSize);

   std::vector<uint8_t> fontBytes_;
   FT_Face face_ = nullptr;
   uint32_t currentPixelSize_ = 0;
};

} // namespace GameEngine
