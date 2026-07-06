#include "CameraGravityBridge.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterJump.h"
#include "../Gravity/GravityBody.h"
#include "../Gravity/PlanetSwitcher.h"
#include "../Vehicle/VehicleGroundMover.h"
#include "Framework/EngineContext.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Scene/Camera/Components/PerlinNoise.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Utility/MathUtils/QuaternionOperations.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {
GameEngine::Vector3 NormalizeOrFallback(
   const GameEngine::Vector3& value,
   const GameEngine::Vector3& fallback) {
   const float length = value.Length();
   if (length > 1e-5f) {
      return value * (1.0f / length);
   }

   const float fallbackLength = fallback.Length();
   if (fallbackLength > 1e-5f) {
      return fallback * (1.0f / fallbackLength);
   }

   return { 0.0f, 1.0f, 0.0f };
}

void TriggerDirectionalShake(
   GameEngine::ICinemachineComponent* cameraComponent,
   const GameEngine::Vector3& direction,
   float amplitude,
   float frequency,
   float duration) {
   if (!cameraComponent) {
      return;
   }

   auto* virtualCamera = cameraComponent->GetOwnerCamera();
   if (!virtualCamera) {
      return;
   }

   auto* noise = virtualCamera->GetComponent<GameEngine::PerlinNoise>();
   if (!noise) {
      noise = virtualCamera->AddComponent<GameEngine::PerlinNoise>();
   }

   noise->ShakeDirectional(direction, amplitude, frequency, duration);
}
}

void CameraGravityBridge::Update(float) {
   // オーナー不在時は更新しない
   if (!HasOwner()) { return; }

   // ワールド座標の取得元（Transform）を参照
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }

   // 惑星中心→自身方向を正規化して重力Upを作る
   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float len = toSelf.Length();
   if (len < 1e-4f) { return; }
   GameEngine::Vector3 gravityUp = toSelf * (1.0f / len);
   GameEngine::Vector3 pos       = transform->transform.translation;
   GameEngine::Vector3 cameraPlanetCenter = planetCenter_;
   if (auto* switcher = GetOwner().GetComponent<PlanetSwitcher>()) {
      float surfaceRadius = 0.0f;
      switcher->TryGetLandingPlanet(cameraPlanetCenter, surfaceRadius);
   }

   // GravityFollowCamera 側へ重力Upと注視対象を同期
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetGravityUp(gravityUp);
      gravityFollowCamera_->SetPivotTarget(pos);
   }

   // PlayerRearFollowCamera 側へ重力Up・注視対象・近傍惑星・前方・空中状態を同期
   if (playerRearFollowCamera_) {
      GameEngine::Vector3 forward = { 0.0f, 0.0f, 1.0f };
      GameEngine::Vector3 playerUp = { 0.0f, 1.0f, 0.0f };
      GameEngine::Quaternion rotation = transform->transform.GetActiveQuaternion();
      forward = GameEngine::RotateVector(forward, rotation);
      playerUp = GameEngine::RotateVector(playerUp, rotation);

      bool isAirborne = false;
      if (auto* jump = GetOwner().GetComponent<CharacterJump>()) {
         isAirborne = jump->IsJumping();
      }

      GameEngine::Vector3 airborneMoveForward = forward;
      if (auto* gravityBody = GetOwner().GetComponent<GravityBody>()) {
         GameEngine::Vector3 velocity = gravityBody->GetVelocity();
         // 重力方向成分を除いた進行方向を使い、上下速度だけでカメラが真上/真下を向くのを避ける。
         GameEngine::Vector3 horizontalVelocity = velocity - gravityUp * velocity.Dot(gravityUp);
         float horizontalSpeed = horizontalVelocity.Length();
         if (horizontalSpeed > 1e-4f) {
            airborneMoveForward = horizontalVelocity * (1.0f / horizontalSpeed);
         }
      }

      playerRearFollowCamera_->SetGravityUp(gravityUp);
      playerRearFollowCamera_->SetPivotTarget(pos);
      playerRearFollowCamera_->SetPlanetCenter(cameraPlanetCenter);
      playerRearFollowCamera_->SetFollowForward(forward);
      playerRearFollowCamera_->SetAirborneMoveForward(airborneMoveForward);
      playerRearFollowCamera_->SetPlayerBasis(forward, playerUp);
      playerRearFollowCamera_->SetAirborne(isAirborne);

      const bool resetHeld =
         isAirborne
         && (GameEngine::EngineContext::IsKeyPressed(GameEngine::KeyCode::R)
            || GameEngine::EngineContext::IsGamePadButtonPressed(GameEngine::GamePadButton::X, 0)
            || GameEngine::EngineContext::IsGamePadButtonPressed(GameEngine::GamePadButton::Y, 0));
      playerRearFollowCamera_->SetAirborneResetHeld(resetHeld);
   }

   // VehicleGroundMover から速度と autoSpeed を取得し、両カメラへ供給する
   float speed     = 0.0f;
   float autoSpeed = 13.0f;
   if (auto* mover = GetOwner().GetComponent<VehicleGroundMover>()) {
      speed     = mover->GetCurrentSpeed();
      autoSpeed = mover->autoSpeed;
   }
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetPlayerSpeed(speed);
      gravityFollowCamera_->SetAutoSpeed(autoSpeed);
   }
   if (playerRearFollowCamera_) {
      playerRearFollowCamera_->SetPlayerSpeed(speed);
      playerRearFollowCamera_->SetAutoSpeed(autoSpeed);
   }

   // PlanetLeashCamera 側へ重力Up・注視対象・惑星中心を同期
   if (planetLeashCamera_) {
      planetLeashCamera_->SetGravityUp(gravityUp);
      planetLeashCamera_->SetPivotTarget(pos);
      planetLeashCamera_->SetSphereCenter(planetCenter_);
   }

   if (auto* landing = GetOwner().GetComponent<CharacterLanding>()) {
      const bool isGrounded = landing->IsGrounded();
      if (enableLandingShake && isGrounded && !wasGrounded_) {
         if (gravityFollowCamera_) {
            TriggerDirectionalShake(
               gravityFollowCamera_,
               NormalizeOrFallback(gravityFollowCamera_->GetCameraUp(), gravityUp),
               landingShakeAmplitude,
               landingShakeFrequency,
               landingShakeDuration);
         }
         if (playerRearFollowCamera_) {
            TriggerDirectionalShake(
               playerRearFollowCamera_,
               NormalizeOrFallback(playerRearFollowCamera_->GetCameraUp(), gravityUp),
               landingShakeAmplitude,
               landingShakeFrequency,
               landingShakeDuration);
         }
         if (planetLeashCamera_) {
            TriggerDirectionalShake(
               planetLeashCamera_,
               NormalizeOrFallback(planetLeashCamera_->GetCameraUp(), gravityUp),
               landingShakeAmplitude,
               landingShakeFrequency,
               landingShakeDuration);
         }
      }
      wasGrounded_ = isGrounded;
   }
}

#ifdef USE_IMGUI
void CameraGravityBridge::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("惑星中心", "Planet Center"),
      planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("%s: %s", Tr("重力追従カメラ", "Gravity Follow Camera"), gravityFollowCamera_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Text("%s: %s", Tr("プレイヤー後方追従", "Player Rear Follow"), playerRearFollowCamera_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Text("%s: %s", Tr("惑星レアッシュカメラ", "Planet Leash Camera"), planetLeashCamera_   ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Separator();
   ImGui::Checkbox(Tr("着地シェイク", "Landing Shake"), &enableLandingShake);
   ImGui::DragFloat(Tr("シェイク振幅", "Shake Amplitude"), &landingShakeAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("シェイク周波数", "Shake Frequency"), &landingShakeFrequency, 0.1f, 0.1f, 60.0f);
   ImGui::DragFloat(Tr("シェイク時間", "Shake Duration"), &landingShakeDuration, 0.01f, 0.0f, 3.0f);
   ImGui::Text("%s: %s", Tr("接地していた", "Was Grounded"), wasGrounded_ ? Tr("はい", "true") : Tr("いいえ", "false"));
}
#endif

nlohmann::json CameraGravityBridge::Serialize() const {
   nlohmann::json json;
   json["enableLandingShake"] = enableLandingShake;
   json["landingShakeAmplitude"] = landingShakeAmplitude;
   json["landingShakeFrequency"] = landingShakeFrequency;
   json["landingShakeDuration"] = landingShakeDuration;
   return json;
}

void CameraGravityBridge::Deserialize(const nlohmann::json& data) {
   if (data.contains("enableLandingShake")) { enableLandingShake = data["enableLandingShake"]; }
   if (data.contains("landingShakeAmplitude")) { landingShakeAmplitude = data["landingShakeAmplitude"]; }
   if (data.contains("landingShakeFrequency")) { landingShakeFrequency = data["landingShakeFrequency"]; }
   if (data.contains("landingShakeDuration")) { landingShakeDuration = data["landingShakeDuration"]; }
}

} // namespace App
