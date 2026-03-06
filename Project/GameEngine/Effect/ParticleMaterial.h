#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <d3d12.h>
#include <wrl.h>

namespace GameEngine {
class Texture;
class GraphicsDevice;

/// @brief パーティクル用マテリアル
class ParticleMaterial {
public:
   /// @brief マテリアルデータ（GPU送信用）
   struct MaterialData {
	  Vector4 color;              // 基本カラー
	  Matrix4x4 uvTransform;      // UV変換行列
   };

   ParticleMaterial();

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param color 初期カラー
   void Create(GraphicsDevice* device, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f));

   /// @brief テクスチャを設定
   void SetTexture(Texture* texture) { texture_ = texture; }

   /// @brief カラーを設定
   void SetColor(const Vector4& color);

   /// @brief UV変換行列を設定
   void SetUVTransform(const Matrix4x4& transform);

   /// @brief テクスチャを取得
   /// @return テクスチャへのポインタ
   Texture* GetTexture() const { return texture_; }

   /// @brief マテリアルデータを取得
   /// @return マテリアルデータへのポインタ
   MaterialData* GetMaterialData() const { return materialData_; }

   /// @brief マテリアルリソースを取得
   /// @return マテリアルリソースへのポインタ
   ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }

private:
   Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
   MaterialData* materialData_ = nullptr;
   Texture* texture_ = nullptr;
};
}
