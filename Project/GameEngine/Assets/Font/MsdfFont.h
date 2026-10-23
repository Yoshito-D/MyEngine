#pragma once

#include "GameEngine/UI/Text/TextTypes.h"
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
      uint32_t codePoint = 0;   ///< 文字を識別するUnicodeコードポイント。
      float advance = 0.0f;     ///< フォントの基準サイズにおける次の文字原点までの送り幅。
      float planeLeft = 0.0f;   ///< グリフ配置矩形の左端。
      float planeBottom = 0.0f; ///< グリフ配置矩形の下端。
      float planeRight = 0.0f;  ///< グリフ配置矩形の右端。
      float planeTop = 0.0f;    ///< グリフ配置矩形の上端。
      float atlasLeft = 0.0f;   ///< アトラス内のグリフ画像の左端。
      float atlasBottom = 0.0f; ///< アトラス内のグリフ画像の下端。
      float atlasRight = 0.0f;  ///< アトラス内のグリフ画像の右端。
      float atlasTop = 0.0f;    ///< アトラス内のグリフ画像の上端。
      bool hasImage = false;    ///< 描画可能なグリフ画像が存在するかどうか。
   };

   void Clear();
   const SourceGlyph* FindSourceGlyph(char32_t codePoint) const;

   GraphicsDevice* device_ = nullptr;                    ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture_; ///< 文字画像を格納する所有GPUテクスチャ。
   /// @brief GPUへのコピー完了まで保持するアップロード用リソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_;
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv_ = {};     ///< フォントアトラスをシェーダーから読むSRVのGPUハンドル。
   UINT descriptorIndex_ = UINT_MAX;               ///< 共有ディスクリプタヒープから借りた番号。UINT_MAXは未割り当て。
   uint32_t atlasWidth_ = 0;                       ///< 文字画像アトラスの横幅（ピクセル）。
   uint32_t atlasHeight_ = 0;                      ///< 文字画像アトラスの高さ（ピクセル）。
   float distanceRange_ = 0.0f;                    ///< アトラスに符号化した符号付き距離の範囲。
   float lineHeight_ = 0.0f;                       ///< フォントの基準行高。
   float ascender_ = 0.0f;                         ///< ベースラインから上端までの距離。
   float descender_ = 0.0f;                        ///< ベースラインから下端までの距離。
   bool bottomOrigin_ = true;                      ///< アトラスのY座標原点が下端にあるかどうか。
   TextAtlasType atlasType_ = TextAtlasType::Msdf; ///< MSDFまたはビットマップなど文字画像の解釈方式。
   std::unordered_map<uint32_t, SourceGlyph> sourceGlyphs_; ///< Unicodeコードポイントから引く元サイズのグリフ情報。
   /// @brief コードポイントと指定サイズに対応する拡縮済みグリフキャッシュ。
   std::unordered_map<uint64_t, GlyphInfo> scaledGlyphs_;
   std::unordered_map<uint64_t, float> kerningPairs_; ///< 隣接グリフ番号の組に対応する字間補正値。
};

} // namespace GameEngine
