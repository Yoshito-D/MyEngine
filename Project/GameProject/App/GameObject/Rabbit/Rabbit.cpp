#include "Rabbit.h"
#include "../Player/Player.h"
#include "../Box/Box.h"
#include "../../Collider/SphereCollider.h"
#include "../../Collider/CollisionLayer.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils.h"
#include "Effect/ParticleSystem.h"
#include <cmath>
#include <numbers>
#include <algorithm>

#ifdef USE_IMGUI
#include "UI/ImGuiManager.h"
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

void Rabbit::Initialize(GameEngine::Model* model, Planet* planet, float radius) {
   model_ = model;
   rabbitRadius_ = radius;

   model_->SetUseQuaternion(true);

   collider_ = std::make_unique<SphereCollider>(this, rabbitRadius_);
   collider_->SetLayer(CollisionLayer::Enemy);

   Vector3 initialPos = model_->GetPosition();
   float initialRadius = planet->GetPlanetRadius() + rabbitRadius_;
   
   gravity_.Initialize(9.8f, rabbitRadius_);
   sphericalMovement_.Initialize(initialPos, planet, initialRadius);
   
   ChooseRandomDirection();
   
   AttachStateMachine();
   InitializeStateMachine();
   
   sphericalMovement_.UpdateModelPosition(model_);
   
   isPlayingCaptureEffect_ = false;
   isCurrentlyFleeing_ = false;
   
   // ラビット専用ポイントライトの作成
   rabbitPointLight_ = EngineContext::CreatePointLight(
	  "RabbitPointLight_" + std::to_string(reinterpret_cast<uintptr_t>(this)),  // ユニークな名前
	  0xffffff88,                      // 白色（少し暗め）
	  initialPos,                       // 初期位置
	  1.5f,                             // 強度
	  4.0f,                             // 半径
	  2.0f                              // 減衰
   );
}

void Rabbit::InitializeStateMachine() {
   if (!stateMachine_) return;

   stateMachine_->AddState("Idle",
      [this]() { OnEnterIdle(); },
      [this]() { OnUpdateIdle(); }
   );

   stateMachine_->AddState("Wandering",
      [this]() { OnEnterWandering(); },
      [this]() { OnUpdateWandering(); }
   );

   stateMachine_->AddState("Fleeing",
      [this]() { OnEnterFleeing(); },
      [this]() { OnUpdateFleeing(); }
   );

   stateMachine_->AddTransitionRule("Idle", {"Wandering", "Fleeing"});
   stateMachine_->AddTransitionRule("Wandering", {"Idle", "Fleeing"});
   stateMachine_->AddTransitionRule("Fleeing", {"Idle", "Wandering"});
}

// ========== Idle State ==========
void Rabbit::OnEnterIdle() {
   idleTimer_ = 0.0f;
   idleTurnTimer_ = 0.0f;
   jumpTimer_ = 0.0f;
   moveDirection_ = Vector3(0.0f, 0.0f, 0.0f);
   isCurrentlyFleeing_ = false;
   fleeTimer_ = 0.0f;
}

void Rabbit::OnUpdateIdle() {
   float dt = GameEngine::EngineContext::GetDeltaTime();
   idleTimer_ += dt;
   idleTurnTimer_ += dt;
   jumpTimer_ += dt;
   
   CheckPlayerDistance();
   
   // プレイヤーが近く、前方にいる場合のみ逃走開始
   if (isPlayerNear_ && IsPlayerInFront()) {
      stateMachine_->RequestState("Fleeing", 10);
      return;
   }
   
   // 2秒間隔で90度振り向く
   if (idleTurnTimer_ >= idleTurnInterval_) {
      idleTurnTimer_ = 0.0f;
      
      Vector3 currentForward = sphericalMovement_.GetForwardDirection();
      Vector3 headDir = sphericalMovement_.GetHeadDirection();
      
      Vector3 forwardTangent = currentForward - headDir * headDir.Dot(currentForward);
      
      if (forwardTangent.Length() > 0.001f) {
         forwardTangent = forwardTangent.Normalize();
         
         float angle = std::numbers::pi_v<float> / 2.0f;
         
         Vector3 newDirection = forwardTangent * std::cos(angle) +
                               headDir.Cross(forwardTangent) * std::sin(angle) +
                               headDir * headDir.Dot(forwardTangent) * (1.0f - std::cos(angle));
         
         targetDirection_ = newDirection.Normalize();
      }
   }
   
   if (targetDirection_.Length() > 0.001f) {
      sphericalMovement_.UpdateOrientation(targetDirection_, turnSpeed_, dt);
   }
   
   // ジャンプ処理
   if (jumpTimer_ >= jumpInterval_ && onGround_) {
      gravity_.Jump(jumpPower_);
      onGround_ = false;
      jumpTimer_ = 0.0f;
   }
   
   // 待機時間終了後、確率で徘徊
   if (idleTimer_ >= idleDuration_) {
      if (RandomUtils::Random(0.0f, 1.0f) < wanderChance_) {
         stateMachine_->RequestState("Wandering", 5);
      } else {
         idleTimer_ = 0.0f;
      }
   }
}

// ========== Wandering State ==========
void Rabbit::OnEnterWandering() {
   wanderTimer_ = 0.0f;
   directionChangeTimer_ = 0.0f;
   jumpTimer_ = 0.0f;
   ChooseRandomDirection();
}

void Rabbit::OnUpdateWandering() {
   float dt = GameEngine::EngineContext::GetDeltaTime();
   wanderTimer_ += dt;
   directionChangeTimer_ += dt;
   jumpTimer_ += dt;
   
   CheckPlayerDistance();
   
   // プレイヤーが近く、前方にいる場合のみ逃走開始
   if (isPlayerNear_ && IsPlayerInFront()) {
      stateMachine_->RequestState("Fleeing", 10);
      return;
   }
   
   // 方向転換（最初の0.5秒間）
   if (directionChangeTimer_ <= 0.5f) {
      sphericalMovement_.UpdateOrientation(targetDirection_, turnSpeed_, dt);
   }
   
   // ジャンプ処理
   if (directionChangeTimer_ >= 0.5f && jumpTimer_ >= jumpInterval_ && onGround_) {
      gravity_.Jump(jumpPower_);
      onGround_ = false;
      jumpTimer_ = 0.0f;
   }
   
   // 空中では移動
   if (!onGround_ && targetDirection_.Length() > 0.001f) {
      UpdateMovement(dt);
   }
   
   // 徘徊時間終了
   if (wanderTimer_ >= wanderDuration_) {
      stateMachine_->RequestState("Idle", 5);
   }
}

// ========== Fleeing State ==========
void Rabbit::OnEnterFleeing() {
   fleeTimer_ = 0.0f;
   jumpTimer_ = 0.0f;
   isCurrentlyFleeing_ = true;
   
   // プレイヤーから逃げる方向を計算し固定
   if (player_ && player_->GetModel()) {
      Vector3 playerPos = player_->GetWorldPosition();
      Vector3 rabbitPos = model_->GetPosition();
      
      Vector3 awayFromPlayer = (rabbitPos - playerPos).Normalize();
      
      Vector3 headDir = sphericalMovement_.GetHeadDirection();
      
      Vector3 tangentAway = awayFromPlayer - headDir * headDir.Dot(awayFromPlayer);
      
      if (tangentAway.Length() > 0.001f) {
         initialFleeDirection_ = tangentAway.Normalize();
         targetDirection_ = initialFleeDirection_;
      } else {
         ChooseRandomDirection();
         initialFleeDirection_ = targetDirection_;
      }

      sphericalMovement_.UpdateOrientation(targetDirection_, turnSpeed_, 0.0f, true);
   }
}

void Rabbit::OnUpdateFleeing() {
   float dt = GameEngine::EngineContext::GetDeltaTime();
   fleeTimer_ += dt;
   jumpTimer_ += dt;
   
   // 距離チェックは行わない - 10秒間必ず逃げ続ける
   // CheckPlayerDistance();  // コメントアウト
   
   // 10秒経過したら必ず待機状態に戻る
   if (fleeTimer_ >= minFleeDuration_) {
      isCurrentlyFleeing_ = false;
      stateMachine_->RequestState("Idle", 5);
      return;
   }
   
   // 球面に沿って直進するために、現在のモデルの正面方向をターゲットにする
   targetDirection_ = sphericalMovement_.GetForwardDirection();

   // 地上では旋回（逃走中は基本的に前を向いているので微調整になります）
   if (onGround_) {
      sphericalMovement_.UpdateOrientation(targetDirection_, turnSpeed_, dt);
   }

   // ジャンプ処理（連続ジャンプ）
   if (jumpTimer_ >= jumpInterval_ && onGround_) {
      gravity_.Jump(jumpPower_);
      onGround_ = false;
      jumpTimer_ = 0.0f;
   }

   // ★修正: 地上・空中に関わらず移動させる
   // "&& !onGround_" を削除しました
   if (targetDirection_.Length() > 0.001f) {
      UpdateMovement(dt);
   }
}

// ========== Common Methods ==========
void Rabbit::CheckPlayerDistance() {
   if (!player_ || !player_->GetModel()) {
      isPlayerNear_ = false;
      return;
   }
   
   Planet* playerPlanet = player_->GetCurrentPlanet();
   Planet* rabbitPlanet = sphericalMovement_.GetCurrentPlanet();
   
   if (playerPlanet != rabbitPlanet) {
      isPlayerNear_ = false;
      return;
   }
   
   Vector3 playerPos = player_->GetWorldPosition();
   Vector3 rabbitPos = model_->GetPosition();
   
   float distance = (playerPos - rabbitPos).Length();
   
   isPlayerNear_ = (distance <= fleeRadius_);
}

bool Rabbit::IsPlayerInFront() const {
   if (!player_ || !player_->GetModel()) {
      return false;
   }
   
   Vector3 playerPos = player_->GetWorldPosition();
   Vector3 rabbitPos = model_->GetPosition();
   
   Vector3 toPlayer = (playerPos - rabbitPos).Normalize();
   
   Vector3 rabbitForward = sphericalMovement_.GetForwardDirection();
   
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   
   Vector3 toPlayerTangent = toPlayer - headDir * headDir.Dot(toPlayer);
   
   if (toPlayerTangent.Length() < 0.001f) {
      return false;
   }
   
   toPlayerTangent = toPlayerTangent.Normalize();
   
   Vector3 forwardTangent = rabbitForward - headDir * headDir.Dot(rabbitForward);
   
   if (forwardTangent.Length() < 0.001f) {
      return false;
   }
   
   forwardTangent = forwardTangent.Normalize();
   
   float dot = forwardTangent.Dot(toPlayerTangent);
   
   return dot > -0.7071f;
}

void Rabbit::ChooseRandomDirection() {
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   
   Vector3 randomDir = Vector3(RandomUtils::Random(-1.0f, 1.0f), RandomUtils::Random(-1.0f, 1.0f), RandomUtils::Random(-1.0f, 1.0f));
   
   Vector3 tangentDir = randomDir - headDir * headDir.Dot(randomDir);
   
   if (tangentDir.Length() > 0.001f) {
      targetDirection_ = tangentDir.Normalize();
   } else {
      targetDirection_ = Vector3(0.0f, 0.0f, 1.0f);
   }
}

void Rabbit::StartCaptureEffect() {
   isPlayingCaptureEffect_ = true;
   
   Vector3 currentPos = model_->GetPosition();
   
   if (captureCallback_) {
      captureCallback_(currentPos);
   }
}

void Rabbit::OnCollisionEnter(GameObject* other) {
   // Boxとの衝突判定 - プレイヤーと同じ処理で捕獲
   class Box* box = dynamic_cast<class Box*>(other);
   if (box) {
	  // プレイヤーのrabbitCaptureCallbackを経由して削除
	  // これにより、GameScene::RemoveRabbitが呼ばれる
	  if (player_) {
		 player_->OnCollisionEnter(this);
	  }
	  return;
   }
}

void Rabbit::OnCollisionStay(GameObject* other) {
   (void)other;
}

void Rabbit::OnCollisionExit(GameObject* other) {
   (void)other;
}

void Rabbit::Update(float dt) {
   if (!model_) return;

   Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
   if (!currentPlanet) return;

   bool isInAir = (gravity_.GetRadialVelocity() != 0.0f);
   float newRadius = gravity_.UpdateGravity(
      currentPlanet,
      model_->GetPosition(),
      sphericalMovement_.GetCurrentRadius(),
      dt,
      isInAir
   );
   sphericalMovement_.SetCurrentRadius(newRadius);
   onGround_ = gravity_.IsOnGround();

   if (stateMachine_) {
      stateMachine_->Update();
   }

   sphericalMovement_.UpdateModelPosition(model_);
   sphericalMovement_.ApplyRotationToModel(model_);
   
   // ポイントライトの位置を更新
   if (rabbitPointLight_) {
	  auto* pointLightData = rabbitPointLight_->GetPointLightData();
	  if (pointLightData) {
		 pointLightData->position = model_->GetPosition();
	  }
   }

#ifdef USE_IMGUI
   ShowDebugInfo();
#endif
}

void Rabbit::UpdateMovement(float dt) {
   if (targetDirection_.Length() < 0.001f) {
      moveDirection_ = Vector3(0.0f, 0.0f, 0.0f);
      return;
   }
   
   float moveDistance = moveSpeed_ * dt;
   sphericalMovement_.Move(targetDirection_, moveDistance);
   
   sphericalMovement_.UpdateOrientation(targetDirection_, turnSpeed_, dt, true);
   
   moveDirection_ = targetDirection_;
}

void Rabbit::ShowDebugInfo() {
#ifdef USE_IMGUI
   ImGui::Begin("Rabbit Debug");
   if (stateMachine_) {
      ImGui::Text("Current State: %s", stateMachine_->GetCurrentState().c_str());
   }
   ImGui::Text("On Ground: %s", onGround_ ? "true" : "false");
   ImGui::Text("Player Near: %s", isPlayerNear_ ? "true" : "false");
   ImGui::Text("Currently Fleeing: %s", isCurrentlyFleeing_ ? "true" : "false");
   ImGui::Text("Flee Timer: %.2f / %.2f", fleeTimer_, minFleeDuration_);
   
   // 逃走中は距離チェック無効であることを表示
   if (isCurrentlyFleeing_) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "FLEEING - Distance check disabled");
   }
   
   ImGui::Text("Player In Front: %s", IsPlayerInFront() ? "true" : "false");
   ImGui::Text("Radial Velocity: %.2f", gravity_.GetRadialVelocity());
   ImGui::Text("Idle Turn Timer: %.2f / %.2f", idleTurnTimer_, idleTurnInterval_);
   ImGui::Text("Jump Timer: %.2f / %.2f", jumpTimer_, jumpInterval_);
   ImGui::Text("Wander Timer: %.2f", wanderTimer_);
   ImGui::Text("Direction Change Timer: %.2f", directionChangeTimer_);
   
   ImGui::Separator();
   ImGui::Text("Target Direction: (%.2f, %.2f, %.2f)", targetDirection_.x, targetDirection_.y, targetDirection_.z);
   ImGui::Text("Initial Flee Direction: (%.2f, %.2f, %.2f)", initialFleeDirection_.x, initialFleeDirection_.y, initialFleeDirection_.z);
   ImGui::Text("Move Direction: (%.2f, %.2f, %.2f)", moveDirection_.x, moveDirection_.y, moveDirection_.z);
   ImGui::Text("Move Speed: %.2f", moveSpeed_);
   
   ImGui::Separator();
   Vector3 rabbitPos = model_->GetPosition();
   ImGui::Text("Rabbit Position: (%.2f, %.2f, %.2f)", rabbitPos.x, rabbitPos.y, rabbitPos.z);
   ImGui::Text("Current Radius: %.2f", sphericalMovement_.GetCurrentRadius());
   
   ImGui::Separator();
   ImGui::Text("Player Pointer: %p", (void*)player_);
   if (player_) {
      ImGui::Text("Player Valid: YES");
      ImGui::Text("Player Model: %p", (void*)player_->GetModel());
      Planet* playerPlanet = player_->GetCurrentPlanet();
      ImGui::Text("Player Planet: %p", (void*)playerPlanet);
      
      Vector3 playerPos = player_->GetWorldPosition();
      ImGui::Text("Player Position: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
      
      Planet* rabbitPlanet = sphericalMovement_.GetCurrentPlanet();
      if (playerPlanet && rabbitPlanet) {
         ImGui::Text("Same Planet: %s", (playerPlanet == rabbitPlanet) ? "YES" : "NO");
         
         float distance = (playerPos - rabbitPos).Length();
         ImGui::Text("Distance to Player: %.2f", distance);
         ImGui::Text("Flee Radius: %.2f (only for start detection)", fleeRadius_);
         
         Vector3 toPlayer = (playerPos - rabbitPos).Normalize();
         Vector3 rabbitForward = sphericalMovement_.GetForwardDirection();
         Vector3 headDir = sphericalMovement_.GetHeadDirection();
         
         Vector3 toPlayerTangent = toPlayer - headDir * headDir.Dot(toPlayer);
         Vector3 forwardTangent = rabbitForward - headDir * headDir.Dot(rabbitForward);
         
         if (toPlayerTangent.Length() > 0.001f && forwardTangent.Length() > 0.001f) {
            toPlayerTangent = toPlayerTangent.Normalize();
            forwardTangent = forwardTangent.Normalize();
            float dot = forwardTangent.Dot(toPlayerTangent);
            float angleRad = std::acos(std::clamp(dot, -1.0f, 1.0f));
            float angleDeg = angleRad * 180.0f / std::numbers::pi_v<float>;
            ImGui::Text("Angle to Player: %.1f degrees (dot: %.2f)", angleDeg, dot);
            ImGui::Text("Field of View: 270 degrees (blind spot: 90 degrees)");
         }
      }
   } else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Player Invalid: NO POINTER");
   }
   
   ImGui::End();
#endif
}
