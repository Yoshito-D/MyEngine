#pragma once

#include "Core/UI/Text/TextTypes.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace GameEngine {
class GraphicsDevice;

/// @brief 同一フォント・同一ピクセルサイズのグリフをGPUテクスチャへ集約する
class GlyphAtlas {
public:
   static constexpr uint32_t kDefaultPageSize = 2048;

   /// @brief グリフアトラスを作成する
   /// @param device グラフィックスデバイス
   /// @param pageSize 正方形アトラス1ページの一辺
   explicit GlyphAtlas(GraphicsDevice* device, uint32_t pageSize = kDefaultPageSize);

   /// @brief GPUテクスチャとSRVを解放する
   ~GlyphAtlas();

   GlyphAtlas(const GlyphAtlas&) = delete;
   GlyphAtlas& operator=(const GlyphAtlas&) = delete;

   /// @brief 登録済みグリフを検索する
   /// @param codePoint Unicodeコードポイント
   /// @return 登録済み情報。未登録の場合はnullptr
   const GlyphInfo* FindGlyph(char32_t codePoint) const;

   /// @brief ラスタライズ済みグリフをアトラスへ登録する
   /// @param codePoint Unicodeコードポイント
   /// @param glyph CPU側グリフ画像とメトリクス
   /// @return 登録結果。登録できなかった場合はnullptr
   const GlyphInfo* AddGlyph(char32_t codePoint, const RasterizedGlyph& glyph);

   /// @brief DirtyなアトラスページをGPUへ転送する
   void FlushPendingUploads();

   /// @brief GPU完了後に不要となったアップロードバッファを解放する
   void ReleaseIntermediateResources();

private:
   struct Page {
      Microsoft::WRL::ComPtr<ID3D12Resource> texture;
      std::vector<uint8_t> pixels;
      D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = {};
      D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = {};
      UINT descriptorIndex = UINT_MAX;
      uint32_t cursorX = 0;
      uint32_t cursorY = 0;
      uint32_t rowHeight = 0;
      bool dirty = false;
      bool uploaded = false;
   };

   Page* CreatePage();
   bool TryAllocate(Page& page, uint32_t width, uint32_t height, uint32_t& outputX, uint32_t& outputY);
   bool UploadPage(Page& page);

   GraphicsDevice* device_ = nullptr;
   uint32_t pageSize_ = kDefaultPageSize;
   std::vector<std::unique_ptr<Page>> pages_;
   std::unordered_map<char32_t, GlyphInfo> glyphs_;
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;
};

} // namespace GameEngine
