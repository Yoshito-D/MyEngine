#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 車特有の移動を担うコンポーネント
///
/// - 接地中: 自動前進（autoSpeed）+ ステアリングによる yaw 回転
///             車体の上方が gravityUp に近いほどスピードブースト
/// - 空中中: 速度変化なし。ステアリングで yaw、前後入力で pitch 回転。
///             回転には慣性（角速度残留）あり
/// - 着地時: SnapToUpVector で即座に正しい姿勢へ補正
class VehicleMover final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleMover";

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 毎フレーム更新（移動処理は外部呼び出し）
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief ステアリング・ピッチ入力と接地状態から移動・姿勢を適用する
   /// @param steerInput  左右入力（-1〜+1）: 地上でyaw、空中でもyaw
   /// @param pitchInput  前後入力（-1〜+1）: 空中でpitchのみ有効
   /// @param isGrounded  接地中なら true
   /// @param gravityUp   現在の重力Up方向
   /// @param deltaTime   フレーム時間
   void ApplyMovement(float steerInput, float pitchInput, bool isGrounded,
					  const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const { return lastMoveDirection_; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 自動前進速度（units/sec）
   float autoSpeed       = 12.0f;

   /// @brief ステアリング角速度（deg/sec）※地上・空中共用
   float steerSpeed      = 90.0f;

   /// @brief ピッチ角速度（deg/sec）※空中のみ
   float pitchSpeed      = 90.0f;

   /// @brief 車体が gravityUp に完全に並行なときの最大加速量（units/sec）
   float boostAmount     = 50.0f;

   /// @brief ブーストが発動する並行度の閾値（0〜1, dot積。1が完全平行）
   float boostThreshold  = 0.9f;

   /// @brief スピードブーストの回復速度（per sec）
   float speedRecovery   = 2.0f;

   /// @brief 空中回転の減衰係数（大きいほど慣性が少ない）
   float angularDamping    = 3.0f;

   /// @brief 着地後の姿勢補正にかける時間（秒）
   float landingAlignTime  = 0.25f;

private:
   /// @brief 直近の移動方向
   GameEngine::Vector3 lastMoveDirection_ = { 0.0f, 0.0f, 1.0f };

   /// @brief 前フレームの接地状態（着地遷移検出用）
   bool  wasGrounded_        = true;

   /// @brief 現在の前進実速度（ブースト込み）— 負値は未初期を示す
   float currentSpeed_       = -1.0f;

   /// @brief 空中の yaw 角速度（rad/sec）
   float angularVelYaw_      = 0.0f;

   /// @brief 空中の pitch 角速度（rad/sec）
   float angularVelPitch_    = 0.0f;

   /// @brief 着地補正タイマー（0以下なら補正中ではない）
   float landingAlignTimer_  = 0.0f;

   /// @brief 着地補正の開始クォータニオン
   GameEngine::Quaternion alignStartRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };

   /// @brief 着地補正の目標クォータニオン（gravityUp に合わせた姿勢）
   GameEngine::Quaternion alignTargetRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
};

} // namespace App
