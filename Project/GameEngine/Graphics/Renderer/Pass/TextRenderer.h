#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include "GameEngine/UI/Text/TextTypes.h"
#include "GameEngine/Math/Types/Transform.h"
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
   bool Initialize(GraphicsDevice* device, const PSOManager* psoManager, FontManager* fontManager);

   /// @brief フレーム単位のCPU描画データをクリアする
   void BeginFrame();

   /// @brief レイアウト済み文字を共有バッファへ追加する
   /// @param layout ローカル座標上の文字配置
   /// @param style 文字色、アンカー、ピボットなどの表示設定
   /// @param transform UI文字オブジェクトのTransform
   /// @param visibleGlyphCount 先頭から表示するグリフ数
   /// @param layoutWidth UIを配置する論理画面幅
   /// @param layoutHeight UIを配置する論理画面高
   /// @param viewportWidth 出力の縦横比を考慮した論理ビューポート幅
   /// @param viewportHeight 出力の縦横比を考慮した論理ビューポート高
   /// @return アトラスページごとの描画コマンドデータ
   std::vector<TextDrawData> QueueText(const TextLayoutResult& layout, const TextStyle& style,
      const Transform& transform, size_t visibleGlyphCount, uint32_t layoutWidth, uint32_t layoutHeight,
      float viewportWidth, float viewportHeight);

   /// @brief CPU上で構築した頂点とインデックスをGPU可視バッファへ反映する
   /// @return 転送に成功した場合はtrue
   bool UploadBuffers();

   /// @brief 指定範囲のテキストを描画する
   /// @param textData QueueTextが返した描画範囲
   /// @param setPipelineFunc Renderer共通のパイプライン設定関数
   void DrawUIText(
      const TextDrawData& textData, const std::function<void(const std::string&, BlendMode)>& setPipelineFunc);

   /// @brief 保持しているGPUリソースを破棄する
   void Finalize();

private:
   struct TextVertex {
      Vector2 position;        ///< 画面に配置した文字頂点の位置（ピクセル）。
      Vector2 texCoord;        ///< テクスチャ座標。
      Vector4 color;           ///< 表示色のRGBA成分。
      Vector4 atlasParameters; ///< xyはアトラス寸法、zは距離範囲、wはMSDFなどのアトラス形式。
   };

   /// @brief 同じフォントアトラスを参照するグリフの描画データ。
   struct PageGeometry {
      D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv = {}; ///< フォントアトラスをシェーダーから読むSRVのGPUハンドル。
      Vector4 atlasParameters = {};              ///< xyはアトラス寸法、zは距離範囲、wはMSDFなどのアトラス形式。
      std::vector<TextVertex> vertices;          ///< このアトラスで描画するグリフの頂点列。
      std::vector<uint32_t> indices;             ///< グリフの三角形を構成する頂点配列への参照番号。
   };

   /// @brief ピクセル座標をクリップ空間へ変換するためのGPU定数。
   struct ViewportData {
      Vector2 size;        ///< 描画先ビューポートの幅・高さ（ピクセル）。
      Vector2 inverseSize; ///< ビューポートの幅・高さの逆数。
   };

   bool EnsureBufferCapacity(size_t vertexCount, size_t indexCount);
   static Vector2 CalculateAnchorPosition(UIAnchor anchor, uint32_t screenWidth, uint32_t screenHeight);
   static Vector2 TransformPoint(const Vector2& point, const Transform& transform, const Vector2& origin);

   GraphicsDevice* device_ = nullptr;                      ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   const PSOManager* psoManager_ = nullptr;                ///< テキスト用パイプラインの設定を参照する非所有ポインター。
   FontManager* fontManager_ = nullptr;                    ///< フォントアセットとグリフ情報を供給する非所有ポインター。
   Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;   ///< 頂点データを保持するバッファ。
   Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;    ///< 三角形の頂点参照番号を保持するバッファ。
   Microsoft::WRL::ComPtr<ID3D12Resource> viewportBuffer_; ///< テキスト描画先のビューポート寸法を保持するバッファ。
   D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};           ///< 頂点バッファを描画入力へ設定するビュー。
   D3D12_INDEX_BUFFER_VIEW indexBufferView_{};             ///< インデックスバッファを描画入力へ設定するビュー。
   std::vector<TextVertex> vertices_;                      ///< メッシュを構成する頂点データ。
   std::vector<uint32_t> indices_;                         ///< 三角形を構成する頂点配列への参照番号。
   size_t vertexCapacity_ = 0;                             ///< 再確保せずに格納できる頂点数。
   size_t indexCapacity_ = 0;                              ///< 再確保せずに格納できるインデックス数。
   float viewportWidth_ = 1.0f;                            ///< 実際の描画ビューポートの横幅（ピクセル）。
   float viewportHeight_ = 1.0f;                           ///< 実際の描画ビューポートの高さ（ピクセル）。
};

} // namespace GameEngine
