#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 重力Up方向へジャンプ初速を付与するコンポーネント
class CharacterJump final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterJump";

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 毎フレーム更新（入力は外部制御のため未使用）
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 未ジャンプ時のみジャンプ速度を付与する
   /// @param gravityUp 現在の重力Up方向
   void Jump(const GameEngine::Vector3& gravityUp);

   /// @brief ジャンプ中かどうかを返す
   bool IsJumping()    const { return isJumping_; }

   /// @brief 着地通知を受けてジャンプ状態を解除する
   void NotifyLanded()       { isJumping_ = false; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief ジャンプ初速の強さ
   float jumpStrength = 3.5f;

private:
   /// @brief ジャンプ中フラグ
   bool isJumping_ = false;
};

} // namespace App
