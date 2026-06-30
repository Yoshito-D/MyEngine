#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 重力方向に追従して姿勢・速度・位置を更新する物理ボディ
class GravityBody final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "GravityBody";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "重力ボディ", "Gravity Body" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 姿勢補間と重力物理更新を実行する
   void Update(float deltaTime) override;

   /// @brief 目標Up方向を設定する（回転補間に使用）
   void SetTargetUpVector(const GameEngine::Vector3& targetUp);

   /// @brief 目標Upへ即座に姿勢スナップする
   void SnapToUpVector(const GameEngine::Vector3& targetUp);

   /// @brief 現在フレームの重力加速度を設定する
   void SetGravity(const GameEngine::Vector3& gravity);

   /// @brief 現在速度を取得する
   GameEngine::Vector3 GetVelocity() const { return velocity_; }

   /// @brief 現在速度を設定する
   void SetVelocity(const GameEngine::Vector3& velocity) { velocity_ = velocity; }

   /// @brief 現在のUp方向を取得する
   GameEngine::Vector3 GetCurrentUpVector() const { return currentUpVector_; }

   /// @brief 今フレームの重力目標Up方向を取得する
   GameEngine::Vector3 GetTargetUpVector() const { return targetUpVector_; }

   /// @brief 現在のUp方向を強制設定する（空中姿勢変化後の着地補正などに使用）
   void SetCurrentUpVector(const GameEngine::Vector3& up) { currentUpVector_ = up.Normalize(); }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief Up補間回転速度
   float rotationSpeed = 5.0f;

   /// @brief 重力強度（加速度係数）
   float gravityStrength = 9.8f;

   /// @brief 重力適用フラグ
   bool  useGravity = true;

private:
   /// @brief 現在Upから目標Upへ姿勢を補間する
   void UpdateRotation(float deltaTime);

   /// @brief 重力加速度で速度・位置を更新する
   void UpdatePhysics(float deltaTime);

private:
   /// @brief 現在のUpベクトル
   GameEngine::Vector3 currentUpVector_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 次に向かうUpベクトル
   GameEngine::Vector3 targetUpVector_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 現在の重力加速度
   GameEngine::Vector3 gravityAcceleration_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 現在の速度
   GameEngine::Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
};

} // namespace App
