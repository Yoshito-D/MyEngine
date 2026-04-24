#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 入力に基づく水平移動・慣性・yaw補間コンポーネント
///
/// - 入力あり: 目標速度に向けて acceleration で加速
/// - 入力なし: friction で減速（慣性）
/// - 位置更新は horizontalVelocity_ ベースで行う
/// - 着地時は ResetHorizontalVelocity() で慣性をゼロにする（CharacterLanding が呼ぶ）
class CharacterWalker final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CharacterWalker";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 移動を実行する（PlayerController から毎フレーム呼ぶ）
   /// @param isGrounded 接地中なら true、空中なら false
   void ApplyMovement(const GameEngine::Vector2& input, const GameEngine::Vector3& gravityUp, float deltaTime, bool isGrounded = true);

   /// @brief 着地時に水平速度をリセットする（CharacterLanding から呼ぶ）
   void ResetHorizontalVelocity() { horizontalVelocity_ = { 0.0f, 0.0f, 0.0f }; }

   GameEngine::Vector3 GetLastMoveDirection()     const { return lastMoveDirection_; }
   GameEngine::Vector3 GetHorizontalVelocity()    const { return horizontalVelocity_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   float moveSpeed       = 5.0f;   ///< 最大移動速度（units/sec）
   float acceleration    = 20.0f;  ///< 地上加速度（units/sec²）
   float friction        = 15.0f;  ///< 地上減速度（units/sec²）— 入力なし時
   float airAcceleration = 5.0f;   ///< 空中加速度（units/sec²）— 小さいほど空中制御が鈍い
   float airFriction     = 0.0f;   ///< 空中減速度（units/sec²）— 0 で慣性が完全に残る
   float turnSpeed       = 10.0f;  ///< 振り向き速度（rad/sec）

private:
   GameEngine::Vector3 horizontalVelocity_ = { 0.0f, 0.0f, 0.0f }; ///< 水平面上の速度
   GameEngine::Vector3 lastMoveDirection_  = { 0.0f, 0.0f, 0.0f };
};

} // namespace App
