#pragma once

#include "Core/UI/Text/TextTypes.h"
#include "FontFace.h"
#include "GlyphAtlas.h"
#include "MsdfFont.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GameEngine {
class GraphicsDevice;

/// @brief FreeTypeフェイス、グリフキャッシュ、テキストレイアウトを統合管理する
class FontManager {
public:
   /// @brief 未初期化のフォントマネージャーを作成する
   FontManager() = default;

   /// @brief FreeTypeとGPUアトラスを解放する
   ~FontManager();

   FontManager(const FontManager&) = delete;
   FontManager& operator=(const FontManager&) = delete;

   /// @brief FreeTypeとGPUアトラス管理を初期化する
   /// @param device グラフィックスデバイス
   /// @return 初期化に成功した場合はtrue
   bool Initialize(GraphicsDevice* device);

   /// @brief フォントを指定IDで読み込む
   /// @param fontId エンジン内で参照するフォントID
   /// @param filePath TTFまたはOTFファイルのパス
   /// @return 読み込みに成功した場合はtrue
   bool LoadFont(const std::string& fontId, const std::filesystem::path& filePath);

   /// @brief msdf-atlas-genのJSONと同名PNGを指定IDで読み込む
   /// @param fontId エンジン内で参照するフォントID
   /// @param jsonPath MSDFメタデータJSONのパス
   /// @return 読み込みに成功した場合はtrue
   bool LoadMsdfFont(const std::string& fontId, const std::filesystem::path& jsonPath);

   /// @brief ディレクトリ以下のTTF・OTF・TTCを再帰的に読み込む
   /// @param directory 検索するフォントディレクトリ
   /// @return 読み込みに成功したフォント数
   size_t LoadFontsFromDirectory(const std::filesystem::path& directory);

   /// @brief ディレクトリ以下のMSDF JSONと同名PNGを再帰的に読み込む
   /// @param directory 検索するフォントディレクトリ
   /// @return 読み込みに成功したMSDFフォント数
   size_t LoadMsdfFontsFromDirectory(const std::filesystem::path& directory);

   /// @brief フォントが読み込まれているか確認する
   /// @param fontId 確認するフォントID
   /// @return 登録済みならtrue
   bool HasFont(const std::string& fontId) const;

   /// @brief 読み込み済みフォントIDを取得する
   /// @return フォントIDのリスト
   std::vector<std::string> GetFontIds() const;

   /// @brief UTF-8文字列を描画用グリフ列へ変換する
   /// @param text UTF-8文字列
   /// @param style レイアウト設定
   /// @return レイアウト済みグリフ列
   TextLayoutResult LayoutText(std::string_view text, const TextStyle& style);

   /// @brief グリフをキャッシュから取得し、未登録なら生成する
   /// @param fontId フォントID
   /// @param pixelSize ピクセルサイズ
   /// @param codePoint Unicodeコードポイント
   /// @return グリフ情報。生成できない場合はnullptr
   const GlyphInfo* GetOrCreateGlyph(const std::string& fontId, uint32_t pixelSize, char32_t codePoint);

   /// @brief 指定フォントサイズの行メトリクスを取得する
   /// @param fontId フォントID
   /// @param pixelSize ピクセルサイズ
   /// @return 行メトリクス
   FontMetrics GetMetrics(const std::string& fontId, uint32_t pixelSize);

   /// @brief 2グリフ間のカーニング量を取得する
   /// @param fontId フォントID
   /// @param leftGlyph 左グリフのインデックス
   /// @param rightGlyph 右グリフのインデックス
   /// @param pixelSize ピクセルサイズ
   /// @return 水平方向のカーニング量
   float GetKerning(const std::string& fontId, uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize);

   /// @brief DirtyなグリフアトラスをGPUへ転送する
   void FlushPendingUploads();

   /// @brief GPU完了済みの中間アップロードリソースを解放する
   void ReleaseIntermediateResources();

   /// @brief フォントの追加・再読み込みを検知するリビジョンを取得する
   /// @return フォント構成が変わるたびに増加する値
   uint64_t GetRevision() const { return revision_; }

   /// @brief フォントとアトラスをすべて破棄する
   void Clear();

private:
   struct AtlasKey {
      std::string fontId;
      uint32_t pixelSize = 0;

      bool operator==(const AtlasKey& other) const {
         return fontId == other.fontId && pixelSize == other.pixelSize;
      }
   };

   struct AtlasKeyHash {
      size_t operator()(const AtlasKey& key) const;
   };

   FontFace* FindFont(const std::string& fontId) const;
   GlyphAtlas* GetOrCreateAtlas(const std::string& fontId, uint32_t pixelSize);

   GraphicsDevice* device_ = nullptr;
   FT_Library library_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<FontFace>> fonts_;
   std::unordered_map<std::string, std::unique_ptr<MsdfFont>> msdfFonts_;
   std::unordered_map<AtlasKey, std::unique_ptr<GlyphAtlas>, AtlasKeyHash> atlases_;
   uint64_t revision_ = 0;
};

} // namespace GameEngine
