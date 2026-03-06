#pragma once
#include "../GameObject.h"
#include "../Planet/Planet.h"
#include "Utility/Math/Quaternion.h"
#include "../../Component/SphericalMovementComponent.h"
#include <memory>

// 前方宣言
class Player;
class Rabbit;

namespace GameEngine {
   class PointLight;
}

/// @brief ボックスゲームオブジェクト
/// プレイヤーのスピンで吹き飛ぶ、押すことができる箱
class Box : public GameObject {
public:
   void Initialize(GameEngine::Model* model, Planet* planet, float size);
   void Update(float dt) override;

   void OnCollisionEnter(GameObject* other) override;
   void OnCollisionStay(GameObject* other) override;
   void OnCollisionExit(GameObject* other) override;
   
   // 所属する惑星を取得
   Planet* GetPlanet() const { return sphericalMovement_.GetCurrentPlanet(); }
   
   // ボックスのサイズを取得
   float GetBoxSize() const { return boxSize_; }
   
   // プレイヤーから押される力を適用
   void ApplyPushForce(const GameEngine::Vector3& direction, float speed, float dt);
   
   // ノックバック力を適用（スピン攻撃用）
   void ApplyKnockback(const GameEngine::Vector3& direction, float power);

   // デバッグ情報表示
   void ShowDebugInfo();

private:
   // プレイヤーとの侵入深さを計算
   float CalculatePenetrationDepth(Player* player);
   
   // コンポーネント
   SphericalMovementComponent sphericalMovement_;
   
   // パラメータ
   float boxSize_ = 1.0f;
   float moveSpeed_ = 0.0f;
   float friction_ = 0.5f;  // 摩擦係数
   float turnSpeed_ = 8.0f;  // 回転速度
   
   // ノックバック
   GameEngine::Vector3 knockbackVelocity_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);
   float knockbackTimer_ = 0.0f;
   float knockbackDuration_ = 1.5f;
   bool isKnockedBack_ = false;
   
   // ボックス専用ポイントライト
   GameEngine::PointLight* boxPointLight_ = nullptr;
   
   // 移動方向
   GameEngine::Vector3 moveDirection_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);
};
