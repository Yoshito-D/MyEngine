#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Object.h"
#include "../Utility/VectorMath.h"
#include "ModelAsset.h"
#include "Component/MeshComponent.h"
#include <vector>
#include <optional>

using namespace Microsoft::WRL;

namespace GameEngine {
class AnimationComponent;
class Material;
class TransformationMatrix;

/// @brief モデルクラス
class Model :public Object {
public:
   /// @brief 空のモデルを生成して描画レジストリへ登録する
   Model();
   /// @brief 描画レジストリから解除して破棄する
   ~Model() override;

   /// @brief モデルのオブジェクト種別を取得する
   /// @return ObjectType::Model
   ObjectType GetObjectType() const override { return ObjectType::Model; }

   /// @brief 描画レジストリ内の全モデルを取得する
   static const std::vector<Model*>& GetRegisteredModels();

   /// @brief 指定モデルを描画レジストリから解除する
   static void UnregisterModel(Model* model);

   /// @brief シーン終了時に描画レジストリを空にする
   static void ClearRegisteredModels() { sRegisteredModels_.clear(); }

   /// @brief モデルの作成
   Model& Create();

   /// ヘルパー関数群

   /// @brief モデルアセットを設定する（ビルダー用便利ラッパー）
   /// @param modelAsset モデルアセット
   Model& SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset);

   /// @brief マテリアルを設定する（ビルダー用便利ラッパー）
   /// @param material マテリアルへのポインタ
   Model& SetMaterial(Material* material);

   /// @brief スキンクラスタを取得する（MeshComponent への便利アクセス）
   SkinCluster* GetSkinCluster() {
	  auto* c = GetComponent<MeshComponent>();
	  return c ? c->GetSkinCluster() : nullptr;
   }
   /// @brief スキンクラスタを読み取り専用で取得する
   const SkinCluster* GetSkinCluster() const {
	  const auto* c = GetComponent<MeshComponent>();
	  return c ? c->GetSkinCluster() : nullptr;
   }

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
   void SetWorldMatrix(const Matrix4x4& worldMatrix);

   /// @brief 親のワールド行列を設定する
   /// @param parentWorldMatrix 親のワールド行列
   void SetParentMatrix(const Matrix4x4& parentMatrix);

   /// @brief 行列の更新
   /// @param camera カメラ
   void UpdateMatrix(Camera* camera);

   /// @brief トランスフォーメーションマトリックスを取得
   TransformationMatrix* GetTransformationMatrix();

private:
   static std::vector<Model*> sRegisteredModels_;
};
}
