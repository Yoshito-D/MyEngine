#pragma once

#include "DrawCommand.h"
#include "Core/UI/Text/TextTypes.h"
#include "Utility/Math/Transform.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace GameEngine {
class FontManager;
class GraphicsDevice;
class PSOManager;

/// @brief グリフ列を共有動的バッファへまとめて描画するUIテキストレンダラー
class TextRenderer {
public:
   /// @brief テキストレンダラーを初期化する
   /// @param device グラフィックスデバイス
   /// @param psoManager パイプライン管理
   /// @param fontManager フォントとアトラスの管理
   bool Initialize(GraphicsDevice* device, PSOManager* psoManager, FontManager* fontManager);

   /// @brief フレーム単位のCPU描画データをクリアする
   void BeginFrame();

   /// @brief レイアウト済み文字を共有バッファへ追加する
   /// @param layout ローカル座標上の文字配置
   /// @param style 文字色、アンカー、ピボットなどの表示設定
   /// @param transform UI文字オブジェクトのTransform
   /// @param visibleGlyphCount 先頭から表示するグリフ数
   /// @param screenWidth 描画対象幅
   /// @param screenHeight 描画対象高さ
   /// @return アトラスページごとの描画コマンドデータ
   std::vector<TextDrawData> QueueText(
      const TextLayoutResult& layout,
      const TextStyle& style,
      const Transform& transform,
      size_t visibleGlyphCount,
      uint32_t screenWidth,
      uint32_t screenHeight);

   /// @brief CPU上で構築した頂点とインデックスをGPU可視バッファへ反映する
   /// @return 転送に成功した場合はtrue
   bool UploadBuffers();

   /// @brief 指定範囲のテキストを描画する
   /// @param textData QueueTextが返した描画範囲
   /// @param setPipelineFunc Renderer共通のパイプライン設定関数
   void DrawUIText(
      const TextDrawData& textData,
      const std::function<void(const std::string&, BlendMode)>& setPipelineFunc);

   /// @brief 保持しているGPUリソースを破棄する
   void Finalize();

private:
   struct TextVertex {
      Vector2 position;
      Vector2 texCoord;
      Vector4 color;
      Vector4 atlasParameters;
   };

   bool EnsureBufferCapacity(size_t vertexCount, size_t indexCount);
   static Vector2 CalculateAnchorPosition(UIAnchor anchor, uint32_t screenWidth, uint32_t screenHeight);
   static Vector2 TransformPoint(const Vector2& point, const Transform& transform, const Vector2& origin);

   GraphicsDevice* device_ = nullptr;
   PSOManager* psoManager_ = nullptr;
   FontManager* fontManager_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
   Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
   Microsoft::WRL::ComPtr<ID3D12Resource> viewportBuffer_;
   D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
   D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
   std::vector<TextVertex> vertices_;
   std::vector<uint32_t> indices_;
   size_t vertexCapacity_ = 0;
   size_t indexCapacity_ = 0;
   uint32_t screenWidth_ = 1;
   uint32_t screenHeight_ = 1;
};

} // namespace GameEngine
