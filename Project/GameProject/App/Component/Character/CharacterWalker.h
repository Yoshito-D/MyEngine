#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 入力に基づく水平移動・慣性・向き補間を担うコンポーネント
///
/// - 入力あり: 目標速度へ加速
/// - 入力なし: 摩擦で減速
/// - 最終的な水平速度で位置更新
class CharacterWalker final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterWalker";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "キャラクター歩行", "Character Walker" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力・重力Up・接地状態から移動を適用する
   /// @param input 移動入力（x:右左, y:前後）
   /// @param gravityUp 現在の重力Up
   /// @param deltaTime フレーム時間
   /// @param isGrounded 接地中なら true
   void ApplyMovement(const GameEngine::Vector2& input, const GameEngine::Vector3& gravityUp, float deltaTime, bool isGrounded = true);

   /// @brief 水平速度をゼロにリセットする
   void ResetHorizontalVelocity() { horizontalVelocity_ = { 0.0f, 0.0f, 0.0f }; }

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection()     const { return lastMoveDirection_; }

   /// @brief 現在の水平速度を取得する
   GameEngine::Vector3 GetHorizontalVelocity()    const { return horizontalVelocity_; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 最大移動速度（units/sec）
   float moveSpeed       = 5.0f;

   /// @brief 地上加速度（units/sec²）
   float acceleration    = 20.0f;

   /// @brief 地上減速度（units/sec²）
   float friction        = 15.0f;

   /// @brief 空中加速度（units/sec²）
   float airAcceleration = 5.0f;

   /// @brief 空中減速度（units/sec²）
   float airFriction     = 0.0f;

   /// @brief 旋回速度（rad/sec）
   float turnSpeed       = 10.0f;

private:
   /// @brief 重力平面上の現在速度
   GameEngine::Vector3 horizontalVelocity_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 直近で有効だった移動方向
   GameEngine::Vector3 lastMoveDirection_  = { 0.0f, 0.0f, 0.0f };
};

} // namespace App
