#include "Planet.h"
#include "../../Collider/SphereCollider.h"
#include "../../Collider/CollisionLayer.h"
#include "Utility/MathUtils.h"
#include <numbers>

using namespace GameEngine;

void Planet::Initialize(Model* model, float gravitationalRadius, float planetRadius) {
   model_ = model;
   gravitationalRadius_ = gravitationalRadius;
   planetRadius_ = planetRadius;
   
   // デフォルトは静止タイプ
   movementParams_.type = MovementType::Static;
   initialPosition_ = model_->GetPosition();
   previousPosition_ = initialPosition_;
   frameMovement_ = Vector3(0.0f, 0.0f, 0.0f);
   elapsedTime_ = 0.0f;
   
   // 重力範囲用の球状コライダーを作成
   collider_ = std::make_unique<SphereCollider>(this, gravitationalRadius_);
   collider_->SetLayer(CollisionLayer::Planet);
}

void Planet::Initialize(Model* model, float gravitationalRadius, float planetRadius, const MovementParams& params) {
   model_ = model;
   gravitationalRadius_ = gravitationalRadius;
   planetRadius_ = planetRadius;
   movementParams_ = params;
   initialPosition_ = model_->GetPosition();
   previousPosition_ = initialPosition_;
   frameMovement_ = Vector3(0.0f, 0.0f, 0.0f);
   elapsedTime_ = params.initialPhase;
   
   // 重力範囲用の球状コライダーを作成
   collider_ = std::make_unique<SphereCollider>(this, gravitationalRadius_);
   collider_->SetLayer(CollisionLayer::Planet);
}

void Planet::Update(float dt) {
   // 前フレームの位置を保存
   previousPosition_ = model_->GetPosition();
   
   elapsedTime_ += dt;
   
   // 移動タイプに応じて位置を更新
   switch (movementParams_.type) {
	  case MovementType::Orbit:
		 UpdateOrbit(dt);
		 break;
	  case MovementType::Pendulum:
		 UpdatePendulum(dt);
		 break;
	  case MovementType::Wave:
		 UpdateWave(dt);
		 break;
	  case MovementType::Static:
	  default:
		 // 静止している場合は何もしない
		 break;
   }
   
   // このフレームの移動量を計算
   Vector3 currentPosition = model_->GetPosition();
   frameMovement_ = currentPosition - previousPosition_;
}

void Planet::UpdateOrbit(float dt) {
   (void)dt;
   // 円軌道運動
   float angle = elapsedTime_ * movementParams_.orbitSpeed;
   
   // 軸の正規化
   Vector3 axis = movementParams_.orbitAxis;
   if (axis.Length() < 0.001f) {
	  axis = Vector3(0.0f, 1.0f, 0.0f);  // デフォルトはY軸
   } else {
	  axis = axis.Normalize();
   }
   
   // 軸に垂直な2つのベクトルを生成
   Vector3 perpendicular1;
   if (std::abs(axis.y) < 0.9f) {
	  perpendicular1 = Vector3(0.0f, 1.0f, 0.0f).Cross(axis).Normalize();
   } else {
	  perpendicular1 = Vector3(1.0f, 0.0f, 0.0f).Cross(axis).Normalize();
   }
   Vector3 perpendicular2 = axis.Cross(perpendicular1).Normalize();
   
   // 円軌道上の位置を計算
   Vector3 offset = perpendicular1 * std::cos(angle) * movementParams_.orbitRadius +
				   perpendicular2 * std::sin(angle) * movementParams_.orbitRadius;
   
   Vector3 newPosition = movementParams_.orbitCenter + offset;
   model_->SetPosition(newPosition);
}

void Planet::UpdatePendulum(float dt) {
   (void)dt;
   // 振り子運動
   float angleInRadians = movementParams_.pendulumAngle * std::numbers::pi_v<float> / 180.0f;
   float angle = std::sin(elapsedTime_ * movementParams_.orbitSpeed) * angleInRadians;
   
   // 振り子の支点から惑星への方向を計算
   Vector3 direction = initialPosition_ - movementParams_.orbitCenter;
   float distance = direction.Length();
   
   if (distance < 0.001f) {
	  distance = movementParams_.orbitRadius;
	  direction = Vector3(0.0f, -1.0f, 0.0f);
   } else {
	  direction = direction.Normalize();
   }
   
   // Z軸周りの回転（簡易実装）
   float x = std::sin(angle) * movementParams_.orbitRadius;
   float y = -std::cos(angle) * movementParams_.orbitRadius;
   
   Vector3 newPosition = movementParams_.orbitCenter + Vector3(x, y, 0.0f);
   model_->SetPosition(newPosition);
}

void Planet::UpdateWave(float dt) {
   (void)dt;
   // 波のような動き
   Vector3 direction = movementParams_.waveDirection;
   if (direction.Length() < 0.001f) {
	  direction = Vector3(1.0f, 0.0f, 0.0f);  // デフォルトはX方向
   } else {
	  direction = direction.Normalize();
   }
   
   // サイン波で移動
   float offset = std::sin(elapsedTime_ * movementParams_.orbitSpeed) * movementParams_.waveAmplitude;
   Vector3 newPosition = initialPosition_ + direction * offset;
   model_->SetPosition(newPosition);
}
