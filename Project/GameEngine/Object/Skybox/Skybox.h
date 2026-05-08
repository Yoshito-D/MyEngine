#pragma once
#include "Object.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "Graphics/Texture.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief スカイボックスクラス
class Skybox : public Object {
public:
   Skybox();
   ~Skybox() override = default;

   /// @brief スカイボックスを作成する
   /// @param device グラフィックスデバイス
   void Create(GraphicsDevice* device);

   /// @brief テクスチャを設定する
   /// @param texture キューブマップテクスチャ
   void SetTexture(Texture* texture);

   /// @brief テクスチャを取得する
   Texture* GetTexture() const { return texture_; }

   /// @brief メッシュを取得する
   const Mesh& GetMesh() const { return mesh_; }

   /// @brief トランスフォーメーションマトリックスリソースを取得する
   ID3D12Resource* GetTransformResource() const;

   /// @brief マテリアルリソースを取得する
   ID3D12Resource* GetMaterialResource() const;

   /// @brief マテリアルの色を設定する (RGBA, 0.0f〜1.0f)
   void SetColor(const Vector4& color);

   /// @brief ビュープロジェクション行列でGPUバッファを更新する (平行移動除去済み)
   void UpdateTransform(const Matrix4x4& viewProjectionMatrix);

private:
   /// @brief VS側: TransformationMatrix (wVP / world / worldInverseTranspose)
   struct SkyboxTransformData {
      Matrix4x4 wVP;
      Matrix4x4 world;
      Matrix4x4 worldInverseTranspose;
   };

   /// @brief PS側: Material
   struct SkyboxMaterialData {
      Vector4 color;
   };

   Mesh mesh_;
   ComPtr<ID3D12Resource> transformResource_ = nullptr;
   SkyboxTransformData* transformData_ = nullptr;

   ComPtr<ID3D12Resource> materialResource_ = nullptr;
   SkyboxMaterialData* materialData_ = nullptr;

   Texture* texture_ = nullptr;
};

} // namespace GameEngine
