#pragma once

#include "GameEngine/Object/Object.h"
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Graphics/Resources/TransformationMatrix.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief スカイボックスクラス
class Skybox : public Object {
public:
   /// @brief 空のスカイボックスを生成して描画レジストリへ登録する
   Skybox();

   /// @brief 描画レジストリから解除してGPUリソースを破棄する
   ~Skybox() override;

   /// @brief スカイボックスのオブジェクト種別を取得する
   /// @return ObjectType::Skybox
   ObjectType GetObjectType() const override { return ObjectType::Skybox; }

   /// @brief 登録済みスカイボックスの一覧を取得する
   static const std::vector<Skybox*>& GetRegisteredSkyboxes();

   /// @brief 指定したスカイボックスを描画レジストリから解除する
   /// @param skybox 解除対象
   static void UnregisterSkybox(Skybox* skybox);

   /// @brief シーン終了時に描画レジストリを空にする
   static void ClearRegisteredSkyboxes() { sRegisteredSkyboxes_.clear(); }

   /// @brief スカイボックスを作成する
   /// @param device グラフィックスデバイス
   void Create(GraphicsDevice* device);

   /// @brief テクスチャを設定する
   /// @param texture キューブマップテクスチャ
   void SetTexture(Texture* texture);

   /// @brief テクスチャを取得する
   Texture* GetTexture() const;

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
      Matrix4x4 wVP;                   ///< ワールド・ビュー・プロジェクション行列。
      Matrix4x4 world;                 ///< ローカル頂点をワールド座標系へ移す変換行列。
      Matrix4x4 worldInverseTranspose; ///< ワールド行列の逆行列の転置行列。
   };

   /// @brief PS側: Material
   struct SkyboxMaterialData {
      Vector4 color; ///< 表示色のRGBA成分。
   };

   static std::vector<Skybox*> sRegisteredSkyboxes_; ///< 自動描画の候補として登録されたSkyboxの非所有一覧。

   Mesh mesh_;                                          ///< メッシュ。
   ComPtr<ID3D12Resource> transformResource_ = nullptr; ///< スカイボックスの変換行列を保持する所有定数バッファ。
   SkyboxTransformData* transformData_ = nullptr;       ///< スカイボックス変換バッファの永続Map先。

   ComPtr<ID3D12Resource> materialResource_ = nullptr; ///< 描画用のマテリアル設定を保持する所有GPUリソース。
   /// @brief 描画用のマテリアル設定を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   SkyboxMaterialData* materialData_ = nullptr;
};

} // namespace GameEngine
