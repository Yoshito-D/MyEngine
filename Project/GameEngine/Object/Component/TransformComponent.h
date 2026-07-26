#pragma once

#include "IObjectComponent.h"
#include "Core/Graphics/TransformationMatrix.h"
#include "MathUtils.h"
#include "Utility/VectorMath.h"
#include <memory>
#include <string>

namespace GameEngine {
/// @brief Objectのローカル変換・親行列・GPU変換バッファを管理する
class TransformComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "TransformComponent";
   static constexpr ComponentDisplayName kDisplayName{ "トランスフォーム", "Transform" };
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

   Transform transform = {}; ///< Objectのローカル変換
   Matrix4x4 parentMatrix = MakeIdentity4x4(); ///< 階層合成に使用する親ワールド行列
   bool useParentMatrix = false; ///< 親行列をワールド行列へ合成するか
   std::string parentObjectName; ///< version 4以前の名前参照を読み込むための互換フィールド

private:
   std::unique_ptr<TransformationMatrix> transformationMatrix_;
   Matrix4x4 worldMatrixOverride_ = MakeIdentity4x4();
   bool hasWorldMatrixOverride_ = false;
};
}
