#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include <string>
#include "Utility/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief メッシュクラス
class Mesh {
public:
   /// @brief 頂点データ構造体
   struct VertexData {
	  Vector4 position; // 頂点位置
	  Vector2 texCoord; // テクスチャ座標
	  Vector3 normal; // 法線ベクトル
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief メッシュの初期化
   /// @param device ID3D12Deviceへのポインタ
   /// @param width 幅
   /// @param height 高さ
   void CreateSprite(float width, float height);

   /// @brief パーティクル用のクワッドメッシュを作成
   /// @param device ID3D12Deviceへのポインタ
   /// @param width 幅
   /// @param height 高さ
   void CreateParticleQuad(float width, float height);

   const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
   const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }
   UINT GetIndexCount() const { return indexCount_; }

   VertexData* GetVertexData() const;
private:
   ComPtr<ID3D12Resource> vertexResource_ = nullptr;
   ComPtr<ID3D12Resource> indexResource_ = nullptr;
   D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
   D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
   VertexData* vertexData_ = nullptr;
   UINT indexCount_ = 0;
};
}