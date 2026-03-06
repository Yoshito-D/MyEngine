#include "Box.h"
#include "../Player/Player.h"
#include "../Rabbit/Rabbit.h"
#include "../../Collider/OBBCollider.h"
#include "../../Collider/CollisionLayer.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils.h"
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "UI/ImGuiManager.h"
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

void Box::Initialize(GameEngine::Model* model, Planet* planet, float size) {
   model_ = model;
   boxSize_ = size;

   model_->SetUseQuaternion(true);

   // OBBコライダー作成（各軸の半分のサイズを指定）
   Vector3 halfSize = Vector3(size * 0.5f, size * 0.5f, size * 0.5f);
   collider_ = std::make_unique<OBBCollider>(this, halfSize);
   collider_->SetLayer(CollisionLayer::Box);
   
   // 初期位置を取得
   Vector3 boxPos = model_->GetPosition();
   Vector3 planetCenter = planet->GetWorldPosition();
   
   // 惑星中心からの距離を計算
   float distance = (boxPos - planetCenter).Length();
   
   // SphericalMovementComponentを初期化
   sphericalMovement_.Initialize(boxPos, planet, distance);
   
   // 初期回転を設定
   sphericalMovement_.ApplyRotationToModel(model_);
   
   // ボックス専用ポイントライトの作成
   boxPointLight_ = EngineContext::CreatePointLight(
	  "BoxPointLight_" + std::to_string(reinterpret_cast<uintptr_t>(this)),  // ユニークな名前
	  0xffffffff,                      // グレー（少し暗め）
	  boxPos,                           // 初期位置
	  2.0f,                             // 強度
	  4.0f,                             // 半径
	  2.0f                              // 減衰
   );
}

void Box::ApplyPushForce(const Vector3& direction, float speed, float dt) {
   if (isKnockedBack_) return;  // ノックバック中は押せない
   
   // 接線方向の移動方向を設定
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   Vector3 tangentDir = direction - headDir * headDir.Dot(direction);
   
   if (tangentDir.Length() > 0.001f) {
      tangentDir = tangentDir.Normalize();
      moveDirection_ = tangentDir;
      moveSpeed_ = speed;
      
      // 移動を適用
      float moveDistance = moveSpeed_ * dt;
      sphericalMovement_.Move(moveDirection_, moveDistance);
      
      // 向きを更新
      sphericalMovement_.UpdateOrientation(moveDirection_, turnSpeed_, dt);
   }
}

void Box::ApplyKnockback(const Vector3& direction, float power) {
   // 接線方向のノックバック力を計算
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   Vector3 tangentDir = direction - headDir * headDir.Dot(direction);
   
   if (tangentDir.Length() > 0.001f) {
      knockbackVelocity_ = tangentDir.Normalize() * power;
      isKnockedBack_ = true;
      knockbackTimer_ = 0.0f;
      moveSpeed_ = 0.0f;  // 通常の移動速度をリセット
   }
}

void Box::Update(float dt) {
   if (!model_) return;

   Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
   if (!currentPlanet) return;
   
   // ノックバック中の処理
   if (isKnockedBack_) {
      knockbackTimer_ += dt;
      
      if (knockbackTimer_ < knockbackDuration_) {
         // 減衰係数を計算
         float decayFactor = 1.0f - (knockbackTimer_ / knockbackDuration_);
         Vector3 currentVelocity = knockbackVelocity_ * decayFactor;
         
         // 移動距離を計算
         float moveDistance = currentVelocity.Length() * dt;
         
         if (moveDistance > 0.001f) {
            Vector3 moveDir = currentVelocity.Normalize();
            
            // SphericalMovementで移動
            sphericalMovement_.Move(moveDir, moveDistance);
            
            // 向きを更新
            sphericalMovement_.UpdateOrientation(moveDir, turnSpeed_, dt);
         }
      } else {
         // ノックバック終了
         isKnockedBack_ = false;
         knockbackVelocity_ = Vector3(0.0f, 0.0f, 0.0f);
      }
   }
   // 通常時の摩擦
   else if (moveSpeed_ > 0.01f) {
      // 摩擦による減速
      moveSpeed_ *= friction_;
      
      if (moveSpeed_ > 0.01f && moveDirection_.Length() > 0.001f) {
         // 慣性で移動
         float moveDistance = moveSpeed_ * dt;
         sphericalMovement_.Move(moveDirection_, moveDistance);
         
         // 向きを更新
         sphericalMovement_.UpdateOrientation(moveDirection_, turnSpeed_, dt);
      } else {
         moveSpeed_ = 0.0f;
      }
   }
   
   // 位置と回転を更新
   sphericalMovement_.UpdateModelPosition(model_);
   sphericalMovement_.ApplyRotationToModel(model_);
   
   // ポイントライトの位置を更新
   if (boxPointLight_) {
	  auto* pointLightData = boxPointLight_->GetPointLightData();
	  if (pointLightData) {
		 pointLightData->position = model_->GetPosition();
	  }
   }

#ifdef USE_IMGUI
   ShowDebugInfo();
#endif
}

void Box::OnCollisionEnter(GameObject* other) {
   // ラビットとの衝突判定 - Rabbit側で処理される
   Rabbit* rabbit = dynamic_cast<Rabbit*>(other);
   if (rabbit) {
      // Rabbit側のOnCollisionEnterが呼ばれるので、ここでは何もしない
      return;
   }
   
   // プレイヤーとの衝突
   Player* player = dynamic_cast<Player*>(other);
   if (player) {
      // 衝突処理はOnCollisionStayで継続的に行う
      return;
   }
}

void Box::OnCollisionStay(GameObject* other) {
   // プレイヤーとの継続的な衝突
   Player* player = dynamic_cast<Player*>(other);
   if (player) {
      // スピン中はノックバック処理
      if (player->IsSpinning()) {
         Vector3 playerPos = player->GetWorldPosition();
         Vector3 boxPos = GetWorldPosition();
         Vector3 knockbackDir = (boxPos - playerPos).Normalize();
         ApplyKnockback(knockbackDir, 15.0f);
      }
      // 通常移動中は押す処理
      else {
         Vector3 playerPos = player->GetWorldPosition();
         Vector3 boxPos = GetWorldPosition();
         Vector3 pushDir = (boxPos - playerPos).Normalize();
         
         // プレイヤーがめり込まないように押し出す
         float penetrationDepth = CalculatePenetrationDepth(player);
         if (penetrationDepth > 0.0f) {
            // プレイヤーを押し出す（Boxは惑星表面に固定されているため、プレイヤーのみ移動）
            Vector3 separationVector = pushDir * penetrationDepth;
            
            // プレイヤーの位置を更新（GameObjectに押し出しメソッドがあれば使用）
            if (player->GetModel()) {
               Vector3 newPlayerPos = playerPos + separationVector;
               player->GetModel()->SetPosition(newPlayerPos);
            }
         }
         
         // プレイヤーの移動速度を取得して適用
         float pushSpeed = 3.0f;  // 押す速度
         float dt = EngineContext::GetDeltaTime();
         ApplyPushForce(pushDir, pushSpeed, dt);
      }
   }
}

void Box::OnCollisionExit(GameObject* other) {
   (void)other;
}

void Box::ShowDebugInfo() {
#ifdef USE_IMGUI
   ImGui::Begin("Box Debug");
   ImGui::Text("Is Knocked Back: %s", isKnockedBack_ ? "true" : "false");
   ImGui::Text("Knockback Timer: %.2f / %.2f", knockbackTimer_, knockbackDuration_);
   ImGui::Text("Move Speed: %.2f", moveSpeed_);
   ImGui::Text("Knockback Velocity: (%.2f, %.2f, %.2f)", 
      knockbackVelocity_.x, knockbackVelocity_.y, knockbackVelocity_.z);
   ImGui::Text("Box Size: %.2f", boxSize_);
   
   ImGui::Separator();
   Vector3 boxPos = model_->GetPosition();
   ImGui::Text("Box Position: (%.2f, %.2f, %.2f)", boxPos.x, boxPos.y, boxPos.z);
   
   Planet* planet = sphericalMovement_.GetCurrentPlanet();
   if (planet) {
      Vector3 planetPos = planet->GetWorldPosition();
      float distance = (boxPos - planetPos).Length();
      float surfaceDistance = distance - planet->GetPlanetRadius();
      ImGui::Text("Distance from Surface: %.2f", surfaceDistance);
      ImGui::Text("Current Radius: %.2f", sphericalMovement_.GetCurrentRadius());
   }
   
   // 回転情報
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   ImGui::Text("Head Direction: (%.2f, %.2f, %.2f)", headDir.x, headDir.y, headDir.z);
   
   Vector3 forwardDir = sphericalMovement_.GetForwardDirection();
   ImGui::Text("Forward Direction: (%.2f, %.2f, %.2f)", forwardDir.x, forwardDir.y, forwardDir.z);
   
   ImGui::End();
#endif
}

float Box::CalculatePenetrationDepth(Player* player) {
   if (!player || !player->GetCollider()) return 0.0f;
   
   // プレイヤーのコライダー（球体）
   Vector3 playerPos = player->GetWorldPosition();
   float playerRadius = 0.65f;  // プレイヤーの半径
   
   // Boxの中心とサイズ
   Vector3 boxCenter = GetWorldPosition();
   Vector3 halfSize = Vector3(boxSize_ * 0.5f, boxSize_ * 0.5f, boxSize_ * 0.5f);
   
   // OBBのローカル空間に球の中心を変換
   Vector3 diff = playerPos - boxCenter;
   
   // OBBの軸を取得
   Quaternion orientation = model_->GetRotationQuaternion();
   Vector3 axisX = RotateVector(Vector3(1.0f, 0.0f, 0.0f), orientation);
   Vector3 axisY = RotateVector(Vector3(0.0f, 1.0f, 0.0f), orientation);
   Vector3 axisZ = RotateVector(Vector3(0.0f, 0.0f, 1.0f), orientation);
   
   // 各軸への投影
   float projX = diff.Dot(axisX);
   float projY = diff.Dot(axisY);
   float projZ = diff.Dot(axisZ);
   
   // 最近接点を計算
   float closestX = std::clamp(projX, -halfSize.x, halfSize.x);
   float closestY = std::clamp(projY, -halfSize.y, halfSize.y);
   float closestZ = std::clamp(projZ, -halfSize.z, halfSize.z);
   
   // 最近接点をワールド座標に戻す
   Vector3 closestPoint = boxCenter + axisX * closestX + axisY * closestY + axisZ * closestZ;
   
   // 球の中心から最近接点までの距離
   float distance = (playerPos - closestPoint).Length();
   
   // 侵入深さ = 球の半径 - 距離
   float penetration = playerRadius - distance;
   return (std::max)(0.0f, penetration);
}
