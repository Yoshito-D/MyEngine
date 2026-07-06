#pragma once

#include "IObjectComponent.h"
#include "Core/Graphics/TransformationMatrix.h"
#include "MathUtils.h"
#include "Utility/VectorMath.h"
#include <memory>
#include <string>

namespace GameEngine {
class TransformComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "TransformComponent";
   static constexpr ComponentDisplayName kDisplayName{ "トランスフォーム", "Transform" };
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
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

   Transform transform = {};
   Matrix4x4 parentMatrix = MakeIdentity4x4();
   bool useParentMatrix = false;
   std::string parentObjectName;

private:
   std::unique_ptr<TransformationMatrix> transformationMatrix_;
   Matrix4x4 worldMatrixOverride_ = MakeIdentity4x4();
   bool hasWorldMatrixOverride_ = false;
};
}
