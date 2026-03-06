#pragma once
#include "Utility/Math/Vector3.h"

class Planet;

/// @brief 重力を管理するコンポーネント
/// 惑星の表面への落下や着地判定を処理
class GravityComponent {
public:
   GravityComponent() = default;
   ~GravityComponent() = default;

   /// @brief 初期化
   /// @param gravity 重力加速度
   /// @param objectRadius オブジェクトの半径（コライダー半径）
   void Initialize(float gravity, float objectRadius);

   /// @brief 重力を更新
   /// @param planet 現在の惑星
   /// @param currentPosition 現在の位置
   /// @param currentRadius 惑星中心からの現在の半径
   /// @param dt デルタタイム
   /// @param isInAir ジャンプ中や落下中の場合true
   /// @return 更新後の半径
   float UpdateGravity(Planet* planet, const GameEngine::Vector3& currentPosition, float currentRadius, float dt, bool isInAir);

   /// @brief 惑星からの引力を適用（惑星切り替え時の慣性付き移動用）
   /// @param planet 引力を与える惑星
   /// @param currentPosition 現在の位置
   /// @param currentVelocity 現在の速度ベクトル（参照渡し、更新される）
   /// @param attractionStrength 引力の強さ
   /// @param dt デルタタイム
   /// @return 引力による速度ベクトル
   GameEngine::Vector3 ApplyPlanetAttraction(Planet* planet, const GameEngine::Vector3& currentPosition, GameEngine::Vector3& currentVelocity, float attractionStrength, float dt);

   /// @brief ジャンプ
   /// @param jumpPower ジャンプ力
   void Jump(float jumpPower);

   /// @brief 地面に接しているか
   bool IsOnGround() const { return onGround_; }

   /// @brief 半径方向の速度を取得
   float GetRadialVelocity() const { return radialVelocity_; }

   /// @brief 半径方向の速度を設定
   void SetRadialVelocity(float velocity) { radialVelocity_ = velocity; }

   /// @brief 重力加速度を取得
   float GetGravity() const { return gravity_; }

   /// @brief 重力加速度を設定
   void SetGravity(float gravity) { gravity_ = gravity; }

private:
   float radialVelocity_ = 0.0f;     // 半径方向の速度（ジャンプ・重力用）
   float gravity_ = 9.8f;            // 重力加速度
   float objectRadius_ = 0.5f;       // オブジェクトの半径
   bool onGround_ = false;           // 地面に接しているか
};
