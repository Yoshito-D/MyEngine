#pragma once

#include "Core/UI/Text/TextTypes.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace GameEngine {
class GraphicsDevice;

/// @brief msdf-atlas-genのJSONとPNGを描画用グリフへ変換するフォントアセット
class MsdfFont final {
public:
   /// @brief 未読み込みのMSDFフォントを作成する
   MsdfFont() = default;

   /// @brief GPUテクスチャとSRVを解放する
   ~MsdfFont();

   MsdfFont(const MsdfFont&) = delete;
   MsdfFont& operator=(const MsdfFont&) = delete;

   /// @brief 同名のJSONとPNGからMSDFフォントを読み込む
   /// @param device グラフィックスデバイス
   /// @param jsonPath msdf-atlas-genが出力したJSON
   /// @return 読み込みに成功した場合はtrue
   bool Load(GraphicsDevice* device, const std::filesystem::path& jsonPath);

   /// @brief 指定サイズへ拡縮したグリフ情報を取得する
   /// @param pixelSize 描画するemサイズ
   /// @param codePoint Unicodeコードポイント
   /// @return グリフ情報。収録されていない場合は代替文字、代替もなければnullptr
   const GlyphInfo* GetGlyph(uint32_t pixelSize, char32_t codePoint);

   /// @brief 指定サイズの行メトリクスを取得する
   /// @param pixelSize 描画するemサイズ
   /// @return ピクセル単位の行メトリクス
   FontMetrics GetMetrics(uint32_t pixelSize) const;

   /// @brief 2グリフ間のカーニング量を取得する
   /// @param leftGlyph 左グリフのUnicode値
   /// @param rightGlyph 右グリフのUnicode値
   /// @param pixelSize 描画するemサイズ
   /// @return ピクセル単位のカーニング量
   float GetKerning(uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize) const;

   /// @brief GPU転送完了後にアップロードバッファを解放する
   void ReleaseIntermediateResources();

private:
   struct SourceGlyph {
      uint32_t codePoint = 0;
      float advance = 0.0f;
      float planeLeft = 0.0f;
      float planeBottom = 0.0f;
      float planeRight = 0.0f;
      float planeTop = 0.0f;
      float atlasLeft = 0.0f;
      float atlasBottom = 0.0f;
      float atlasRight = 0.0f;
      float atlasTop = 0.0f;
      bool hasImage = false;
   };

   void Clear();
   const SourceGlyph* FindSourceGlyph(char32_t codePoint) const;

   GraphicsDevice* device_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture_;
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_;
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv_ = {};
   UINT descriptorIndex_ = UINT_MAX;
   uint32_t atlasWidth_ = 0;
   uint32_t atlasHeight_ = 0;
   float distanceRange_ = 0.0f;
   float lineHeight_ = 0.0f;
   float ascender_ = 0.0f;
   float descender_ = 0.0f;
   bool bottomOrigin_ = true;
   TextAtlasType atlasType_ = TextAtlasType::Msdf;
   std::unordered_map<uint32_t, SourceGlyph> sourceGlyphs_;
   std::unordered_map<uint64_t, GlyphInfo> scaledGlyphs_;
   std::unordered_map<uint64_t, float> kerningPairs_;
};

} // namespace GameEngine
