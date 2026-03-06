#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Object.h"
#include "../Utility/VectorMath.h"
#include "ModelAsset.h"

using namespace Microsoft::WRL;

namespace GameEngine {

/// @brief モデルクラス
class Model :public Object {
public:

   /// @brief モデルの作成
   /// @param modelAsset モデルアセット
   /// @param material マテリアル
   void Create(ModelAsset* modelAsset = nullptr, Material* material = nullptr);

   /// @brief モデルアセットを取得する
   /// @return モデルアセットへのポインタ
   ModelAsset* GetModelAsset() const { return modelAsset_; }

   /// @brief モデルアセットを設定する
   /// @param modelAsset モデルアセットへのポインタ
   void SetModelAsset(ModelAsset* modelAsset) { modelAsset_ = modelAsset; }

   /// @brief モデルの位置を取得する
   /// @return 位置
   const Vector3& GetPosition() const { return transform_.translation; }

   /// @brief モデルの回転を取得する
   /// @return 回転
   const Vector3& GetRotation() const { return transform_.rotation; }

   /// @brief モデルのスケールを取得する
   /// @return スケール
   const Vector3& GetScale() const { return transform_.scale; }

   /// @brief モデルのトランスフォームを設定する
   /// @param transform トランスフォーム
   void SetTransform(const Transform& transform) { transform_ = transform; }

   /// @brief モデルの位置を設定する
   /// @param translation 位置
   void SetPosition(const Vector3& translation) { transform_.translation = translation; }

   /// @brief モデルの回転を設定する
   /// @param rotation 回転
   void SetRotation(const Vector3& rotation) { transform_.rotation = rotation; }

   /// @brief モデルのスケールを設定する
   /// @param scale スケール
   void SetScale(const Vector3& scale) { transform_.scale = scale; }

   /// @brief Quaternionを使用して回転を設定する
   /// @param quaternion 回転を表すQuaternion
   void SetRotationQuaternion(const Quaternion& quaternion) {
	  quaternion_ = quaternion;
	  useQuaternion_ = true;
   }

   /// @brief Quaternionを取得する
   /// @return 現在のQuaternion
   const Quaternion& GetRotationQuaternion() const { return quaternion_; }

   /// @brief Quaternionを使用するかどうかを設定する
   /// @param use trueならQuaternion、falseならEuler角を使用
   void SetUseQuaternion(bool use) { useQuaternion_ = use; }

   /// @brief Quaternionを使用しているかどうかを取得する
   /// @return trueならQuaternion使用中
   bool IsUsingQuaternion() const { return useQuaternion_; }

   /// @brief ワールド行列を設定する
   /// @param worldMatrix ワールド行列
   void SetWorldMatrix(const Matrix4x4& worldMatrix) { worldMatrix_ = worldMatrix; }

   /// @brief 親のワールド行列を設定する
   /// @param parentWorldMatrix 親のワールド行列
   void SetParentMatrix(const Matrix4x4& parentMatrix) { parentMatrix_ = parentMatrix; }

   /// @brief 行列の更新
   /// @param camera カメラ
   void UpdateMatrix(Camera* camera);
private:
   ModelAsset* modelAsset_ = nullptr;
   Quaternion quaternion_ = Quaternion::Identity();
   bool useQuaternion_ = false;
};
}