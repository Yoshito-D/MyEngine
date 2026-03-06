#pragma once
#include "GraphicsDevice.h"
#include "Mesh.h"
#include "Material.h"
#include "DirectionalLight.h"
#include "TransformationMatrix.h"
#include <memory>
#include <functional>

namespace GameEngine {
class Camera;

class Object {
public:
   /// @brief オブジェクトの初期化
   /// @param device グラフィックスデバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief トランスフォーメーションマトリックスを取得
   TransformationMatrix* GetTransformationMatrix() { return transformationMatrix_.get(); }

   /// @brief マテリアルのリストを取得する
   /// @return マテリアルのリストへの参照
   const std::vector<Material*>& GetMaterials() const { return materials_; }

   /// @brief マテリアルを設定する
   /// @param material マテリアル（単一マテリアル用）
   void SetMaterial(Material* material);

   /// @brief マテリアルを追加する
   /// @param material 追加するマテリアル
   void AddMaterial(Material* material);

   /// @brief マテリアルを設定する（マルチマテリアル対応）
   /// @param materials マテリアルのリスト
   void SetMaterials(const std::vector<Material*>& materials);

   /// @brief オブジェクトのトランスフォームを設定する
   /// @return トランスフォーム
   Transform GetTransform() const { return transform_; }

   /// @brief 指定インデックスのマテリアルを取得する
   /// @param index マテリアルのインデックス（省略時は0）
   /// @return マテリアルへのポインタ
   Material* GetMaterial(size_t index = 0) const;

   /// @brief マテリアルの数を取得する
   /// @return マテリアルの数
   size_t GetMaterialCount() const { return materials_.size(); }

   /// @brief 親の行列を使用するか設定する
   /// @param isUsing 親のワールド行列を使用する場合はtrue、使用しない場合はfalse
   void SetIsUsingParentMatrix(bool isUsing) { isUsingParentMatrix_ = isUsing; }

   /// @brief メッシュを取得する
   /// @return メッシュへのポインタ
   Mesh* GetMesh() const { return mesh_.get(); }
protected:
   enum class MeshType {
	  Sprite,
   };

   std::unique_ptr<Mesh> mesh_ = nullptr;
   std::vector<Material*> materials_;
   std::unique_ptr<TransformationMatrix> transformationMatrix_ = nullptr;
   Transform transform_ = {};
   Matrix4x4 worldMatrix_ = MakeIdentity4x4();
   Matrix4x4 parentMatrix_ = MakeIdentity4x4();
   bool isUsingParentMatrix_ = false;
protected:

   void CreateMesh();

   void CreateTransformationMatrix();

   void SetCreateMeshFunction(std::function<void()> func) { createMeshFunc_ = func; }
private:
   std::function<void()> createMeshFunc_;
};
}