#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Object.h"
#include "../Utility/VectorMath.h"
#include "ModelAsset.h"
#include <vector>
#include <memory>
#include <optional>

using namespace Microsoft::WRL;

namespace GameEngine {
class AnimationComponent;
class Material;

/// @brief モデルクラス
class Model :public Object {
public:
   Model();
   ~Model() override;

   static const std::vector<Model*>& GetRegisteredModels();

   /// @brief モデルの作成
   /// @param modelAsset モデルアセット
   /// @param material マテリアル
   void Create(const std::shared_ptr<ModelAsset>& modelAsset = {}, Material* material = nullptr);

   /// @brief モデルアセットを取得する
   /// @return モデルアセットへのポインタ
   ModelAsset* GetModelAsset() const { return modelAsset_.get(); }

   /// @brief モデルアセットハンドルを取得する
   const std::shared_ptr<ModelAsset>& GetModelAssetHandle() const { return modelAsset_; }

   /// @brief モデルアセットを設定する
   /// @param modelAsset モデルアセットへのポインタ
   void SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset);

   /// @brief モデル単位のスキンクラスタを取得
   SkinCluster* GetSkinCluster();
   const SkinCluster* GetSkinCluster() const;

   /// @brief モデルの位置を取得する
   /// @return 位置
   const Vector3& GetPosition() const;

   /// @brief モデルの回転を取得する
   /// @return 回転
   const Vector3& GetRotation() const;

   /// @brief モデルのスケールを取得する
   /// @return スケール
   const Vector3& GetScale() const;

   /// @brief モデルのトランスフォームを設定する
   /// @param transform トランスフォーム
   void SetTransform(const Transform& transform);

   /// @brief モデルの位置を設定する
   /// @param translation 位置
   void SetPosition(const Vector3& translation);

   /// @brief モデルの回転を設定する
   /// @param rotation 回転
   void SetRotation(const Vector3& rotation);

   /// @brief モデルのスケールを設定する
   /// @param scale スケール
   void SetScale(const Vector3& scale);

   /// @brief Quaternionを使用して回転を設定する
   /// @param quaternion 回転を表すQuaternion
   void SetRotationQuaternion(const Quaternion& quaternion);

   /// @brief Quaternionを取得する
   /// @return 現在のQuaternion
   const Quaternion& GetRotationQuaternion() const;

   /// @brief Quaternionを使用するかどうかを設定する
   /// @param use trueならQuaternion、falseならEuler角を使用
   void SetUseQuaternion(bool use);

   /// @brief Quaternionを使用しているかどうかを取得する
   /// @return trueならQuaternion使用中
   bool IsUsingQuaternion() const;

   /// @brief ワールド行列を設定する
   /// @param worldMatrix ワールド行列
   void SetWorldMatrix(const Matrix4x4& worldMatrix) { worldMatrixOverride_ = worldMatrix; hasWorldMatrixOverride_ = true; }

   /// @brief 親のワールド行列を設定する
   /// @param parentWorldMatrix 親のワールド行列
   void SetParentMatrix(const Matrix4x4& parentMatrix);

   /// @brief 行列の更新
   /// @param camera カメラ
   void UpdateMatrix(Camera* camera);
private:
   static std::vector<Model*> sRegisteredModels_;

   std::shared_ptr<ModelAsset> modelAsset_;
   std::optional<SkinCluster> skinCluster_;
   Matrix4x4 worldMatrixOverride_ = MakeIdentity4x4();
   bool hasWorldMatrixOverride_ = false;
};
}