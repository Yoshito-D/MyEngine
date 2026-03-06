#pragma once
#include "Utility/Math/Quaternion.h"
#include "Utility/Math/Vector3.h"
#include "Object/Model/Model.h"

class Planet;

/// @brief 球面移動を管理するコンポーネント
/// プレイヤーやラビットなどが惑星の表面を移動する際に使用
class SphericalMovementComponent {
public:
   SphericalMovementComponent() = default;
   ~SphericalMovementComponent() = default;

   /// @brief 初期化
   /// @param initialPosition 初期位置
   /// @param planet 所属する惑星
   /// @param radius 惑星表面からの距離
   void Initialize(const GameEngine::Vector3& initialPosition, Planet* planet, float radius);

   /// @brief 球面上を移動
   /// @param tangentDirection 接平面上の移動方向（正規化済み）
   /// @param moveDistance 移動距離
   void Move(const GameEngine::Vector3& tangentDirection, float moveDistance);

   /// @brief 位置Quaternionから実際のワールド座標を計算してモデルに適用
   /// @param model 適用先のモデル
   void UpdateModelPosition(GameEngine::Model* model);

   /// @brief 向きQuaternionを更新（接平面上の目標方向に向ける）
   /// @param targetTangentDirection 目標方向（接平面上）
   /// @param turnSpeed 回転速度（ラジアン/秒）
   /// @param dt デルタタイム
   /// @param immediate trueの場合は即座に向きを変更
   void UpdateOrientation(const GameEngine::Vector3& targetTangentDirection, float turnSpeed, float dt, bool immediate = false);

   /// @brief モデルに最終的な回転を適用
   /// @param model 適用先のモデル
   void ApplyRotationToModel(GameEngine::Model* model);

   /// @brief 頭の方向を取得（惑星の法線方向）
   GameEngine::Vector3 GetHeadDirection() const;

   /// @brief 前方向を取得（キャラクターが向いている方向）
   GameEngine::Vector3 GetForwardDirection() const;

   /// @brief 現在の惑星中心からの半径を取得
   float GetCurrentRadius() const { return currentRadius_; }

   /// @brief 現在の惑星中心からの半径を設定
   void SetCurrentRadius(float radius) { currentRadius_ = radius; }

   /// @brief 位置Quaternionを取得
   GameEngine::Quaternion GetPositionQuaternion() const { return positionQuaternion_; }

   /// @brief 位置Quaternionを設定
   void SetPositionQuaternion(const GameEngine::Quaternion& quat) { positionQuaternion_ = quat; }

   /// @brief 向きQuaternionを取得
   GameEngine::Quaternion GetOrientationQuaternion() const { return orientationQuaternion_; }

   /// @brief 向きQuaternionを設定
   void SetOrientationQuaternion(const GameEngine::Quaternion& quat) { orientationQuaternion_ = quat; }

   /// @brief 現在の惑星を取得
   Planet* GetCurrentPlanet() const { return currentPlanet_; }

   /// @brief 現在の惑星を設定
   void SetCurrentPlanet(Planet* planet) { currentPlanet_ = planet; }

private:
   // 位置を表すQuaternion（惑星中心からの方向、頭の向き）
   GameEngine::Quaternion positionQuaternion_ = GameEngine::Quaternion::Identity();

   // キャラクターの向きを表すQuaternion（Y軸周りの回転）
   GameEngine::Quaternion orientationQuaternion_ = GameEngine::Quaternion::Identity();

   // 惑星中心からの距離（半径）
   float currentRadius_ = 0.0f;

   // 現在所属している惑星
   Planet* currentPlanet_ = nullptr;
};
