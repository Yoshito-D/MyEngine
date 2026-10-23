#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Graphics/Resources/TransformationMatrix.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Math/VectorMath.h"
#include <memory>
#include <string>
#include <span>

namespace GameEngine {

/// @brief Objectのローカル変換・親行列・GPU変換バッファを管理する
class TransformComponent final : public IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "TransformComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"トランスフォーム", "Transform"};

   /// @copydoc IObjectComponent::GetTypeName
   const char* GetTypeName() const override;

   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// @brief GPUへ送るトランスフォーム行列バッファを必要に応じて作成して取得する
   /// @return トランスフォーム行列バッファ
   TransformationMatrix* EnsureTransformationMatrix();

   /// @brief GPUへ送るトランスフォーム行列バッファを取得する
   /// @return トランスフォーム行列バッファ。未作成ならnullptr
   TransformationMatrix* GetTransformationMatrix() { return transformationMatrix_.get(); }

   /// @brief GPUへ送るトランスフォーム行列バッファを取得する
   /// @return トランスフォーム行列バッファ。未作成ならnullptr
   const TransformationMatrix* GetTransformationMatrix() const { return transformationMatrix_.get(); }

   /// @brief 自動生成されるワールド行列の代わりに使用する行列を設定する
   /// @param worldMatrix 上書き用ワールド行列
   void SetWorldMatrixOverride(const Matrix4x4& worldMatrix);

   /// @brief ワールド行列の上書きを解除する
   void ClearWorldMatrixOverride();

   /// @brief ワールド行列の上書きが設定されているか取得する
   /// @return 上書きが有効ならtrue
   bool HasWorldMatrixOverride() const { return hasWorldMatrixOverride_; }

   /// @brief 上書き用ワールド行列を取得する
   /// @return 上書き用ワールド行列
   const Matrix4x4& GetWorldMatrixOverride() const { return worldMatrixOverride_; }

   /// @brief 計算・表示用にローカル姿勢を読み取る。
   const Transform& GetLocalPose() const { return transform; }

   /// @brief 有限なローカル姿勢を一括適用し、外部行列の上書きを解除する。
   bool ApplyLocalPose(const Transform& pose);

   /// @brief 所有Objectの現在の親を解決してローカル行列へ合成する。
   Matrix4x4 ComposeWorldMatrix(const Matrix4x4& local) const;

private:
   Transform transform;                                         ///< 親の座標系に対する位置・回転・拡縮の正本。
   std::unique_ptr<TransformationMatrix> transformationMatrix_; ///< 描画に使う変換行列とGPU定数バッファ。
   Matrix4x4 worldMatrixOverride_ = MakeIdentity4x4();          ///< 上書き用ワールド行列。
   bool hasWorldMatrixOverride_ = false;                        ///< ワールド行列の上書きが設定されているかどうかする。
};
}
