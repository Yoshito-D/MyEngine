#include "CameraGravityBridge.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterJump.h"
#include "../Gravity/GravityBody.h"
#include "../Gravity/PlanetSwitcher.h"
#include "../Vehicle/VehicleGroundMover.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Scene/Camera/Components/PerlinNoise.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {
constexpr float kLandingPredictionStepSeconds = 1.0f / 60.0f;
constexpr float kLandingPredictionDirectionEpsilon = 1e-4f;

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

struct LandingPrediction {
   GameEngine::Vector3 up = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 backward = { 0.0f, 0.0f, -1.0f };
   GameEngine::Vector3 contactPoint = { 0.0f, 0.0f, 0.0f };
   float secondsToImpact = 0.0f;
};

float ComputeObbSupportRadius(
   const GameEngine::Quaternion& rotation,
   const GameEngine::Vector3& halfExtents,
   const GameEngine::Vector3& surfaceUp) {
   const GameEngine::Vector3 axisX =
	  GameEngine::RotateVector({ 1.0f, 0.0f, 0.0f }, rotation);
   const GameEngine::Vector3 axisY =
	  GameEngine::RotateVector({ 0.0f, 1.0f, 0.0f }, rotation);
   const GameEngine::Vector3 axisZ =
	  GameEngine::RotateVector({ 0.0f, 0.0f, 1.0f }, rotation);
   return std::abs(axisX.Dot(surfaceUp)) * std::abs(halfExtents.x)
	  + std::abs(axisY.Dot(surfaceUp)) * std::abs(halfExtents.y)
	  + std::abs(axisZ.Dot(surfaceUp)) * std::abs(halfExtents.z);
}

bool PredictLanding(
   const GameEngine::Vector3& position,
   const GameEngine::Vector3& velocity,
   const GameEngine::Vector3& planetCenter,
   float surfaceRadius,
   float landingOffset,
   const GameEngine::Vector3& obbHalfExtents,
   const GameEngine::Quaternion& playerRotation,
   float gravityStrength,
   float predictionHorizon,
   const GameEngine::Vector3& fallbackForward,
   const GameEngine::Vector3& fallbackRight,
   LandingPrediction& outPrediction) {
   float horizon = std::clamp(predictionHorizon, 0.0f, 5.0f);
   if (horizon <= 1e-4f || surfaceRadius <= 1e-4f) {
	  return false;
   }

   GameEngine::Vector3 simulatedPosition = position;
   GameEngine::Vector3 simulatedVelocity = velocity;
   GameEngine::Vector3 initialToSelf = simulatedPosition - planetCenter;
   float initialDistance = initialToSelf.Length();
   if (initialDistance <= 1e-4f) {
	  return false;
   }

   GameEngine::Vector3 initialUp = initialToSelf * (1.0f / initialDistance);
   float initialSnapRadius =
	  surfaceRadius
	  + landingOffset
	  + ComputeObbSupportRadius(playerRotation, obbHalfExtents, initialUp);
   float previousClearance = initialDistance - initialSnapRadius;
   if (previousClearance <= 0.0f) {
	  return false;
   }

   float elapsed = 0.0f;
   while (elapsed < horizon) {
	  float step = std::min(kLandingPredictionStepSeconds, horizon - elapsed);
	  GameEngine::Vector3 toSelf = simulatedPosition - planetCenter;
	  float distance = toSelf.Length();
	  if (distance <= 1e-4f) {
		 return false;
	  }

	  GameEngine::Vector3 surfaceUp = toSelf * (1.0f / distance);
	  GameEngine::Vector3 previousVelocity = simulatedVelocity;
	  GameEngine::Vector3 previousPosition = simulatedPosition;

	  // GravityBody と同じ半陰的オイラー順序で、候補惑星の放射重力だけを短時間先読みする。
	  simulatedVelocity +=
		 surfaceUp * (-std::max(0.0f, gravityStrength) * step);
	  simulatedPosition += simulatedVelocity * step;

	  GameEngine::Vector3 nextToSelf = simulatedPosition - planetCenter;
	  float nextDistance = nextToSelf.Length();
	  if (nextDistance <= 1e-4f) {
		 return false;
	  }
	  GameEngine::Vector3 nextUp = nextToSelf * (1.0f / nextDistance);
	  float nextSnapRadius =
		 surfaceRadius
		 + landingOffset
		 + ComputeObbSupportRadius(playerRotation, obbHalfExtents, nextUp);
	  float nextClearance = nextDistance - nextSnapRadius;

	  if (nextClearance <= 0.0f) {
		 float denominator = previousClearance - nextClearance;
		 float crossingAlpha = denominator > 1e-5f
			? std::clamp(previousClearance / denominator, 0.0f, 1.0f)
			: 1.0f;
		 GameEngine::Vector3 hitPosition =
			previousPosition + (simulatedPosition - previousPosition) * crossingAlpha;
		 GameEngine::Vector3 hitVelocity =
			previousVelocity + (simulatedVelocity - previousVelocity) * crossingAlpha;
		 GameEngine::Vector3 hitUp = NormalizeOrFallback(hitPosition - planetCenter, nextUp);

		 // 地表へ近づく交差だけを着地予測として扱う。
		 if (hitVelocity.Dot(hitUp) > 0.0f) {
			return false;
		 }

		 GameEngine::Vector3 landingForward =
			hitVelocity - hitUp * hitVelocity.Dot(hitUp);
		 if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
			landingForward =
			   fallbackForward - hitUp * fallbackForward.Dot(hitUp);
		 }
		 if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
			// 真上・真下を向いた垂直着地では機首投影も退化するため、
			// 表示中のカメラRightから画面上の前方を復元して方位を維持する。
			landingForward = fallbackRight.Cross(hitUp);
		 }
		 if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
			return false;
		 }

		 landingForward = NormalizeOrFallback(landingForward, fallbackForward);
		 outPrediction.up = hitUp;
		 outPrediction.backward = -landingForward;
		 outPrediction.contactPoint =
			planetCenter + hitUp * (surfaceRadius + landingOffset);
		 outPrediction.secondsToImpact = elapsed + step * crossingAlpha;
		 return true;
	  }

	  previousClearance = nextClearance;
	  elapsed += step;
   }

   return false;
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

void CameraGravityBridge::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   gravityFollowCamera_ = nullptr;
   playerRearFollowCamera_ = nullptr;
   planetLeashCamera_ = nullptr;

   if (auto* camera = sceneWorld.FindVirtualCamera(gravityFollowCameraId_)) {
      gravityFollowCamera_ = camera->GetComponent<GravityFollowCamera>();
   }
   if (auto* camera = sceneWorld.FindVirtualCamera(playerRearFollowCameraId_)) {
      playerRearFollowCamera_ = camera->GetComponent<PlayerRearFollowCamera>();
   }
   if (auto* camera = sceneWorld.FindVirtualCamera(planetLeashCameraId_)) {
      planetLeashCamera_ = camera->GetComponent<PlanetLeashCamera>();
   }
}

void CameraGravityBridge::Update(float) {
   // オーナー不在時は更新しない
   if (!HasOwner()) { return; }

   // ワールド座標の取得元（Transform）を参照
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* landing = GetOwner().GetComponent<CharacterLanding>();
   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();

   // 惑星中心→自身方向を正規化して重力Upを作る
   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float len = toSelf.Length();
   if (len < 1e-4f) { return; }
   GameEngine::Vector3 gravityUp = toSelf * (1.0f / len);
   GameEngine::Vector3 pos       = transform->transform.translation;
   GameEngine::Vector3 cameraPlanetCenter = planetCenter_;
   float cameraPlanetSurfaceRadius = 0.0f;
   bool hasCameraPlanet = false;
   if (switcher) {
      hasCameraPlanet =
		 switcher->TryGetLandingPlanet(cameraPlanetCenter, cameraPlanetSurfaceRadius);
   }

   // GravityFollowCamera 側へ重力Upと注視対象を同期
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetGravityUp(gravityUp);
      gravityFollowCamera_->SetPivotTarget(pos);
   }

   // PlayerRearFollowCamera 側へ重力Up・注視対象・近傍惑星・前方・空中状態を同期
   if (playerRearFollowCamera_) {
      GameEngine::Vector3 forward = { 0.0f, 0.0f, 1.0f };
      GameEngine::Quaternion rotation = transform->transform.GetActiveQuaternion();
      forward = GameEngine::RotateVector(forward, rotation);

      bool isAirborne = false;
      if (auto* jump = GetOwner().GetComponent<CharacterJump>()) {
         isAirborne = jump->IsJumping();
      }

      GameEngine::Vector3 airborneMoveForward = forward;
      GameEngine::Vector3 playerVelocity = { 0.0f, 0.0f, 0.0f };
      if (gravityBody) {
         GameEngine::Vector3 velocity = gravityBody->GetVelocity();
         playerVelocity = velocity;
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
      playerRearFollowCamera_->SetAirborne(isAirborne);
      playerRearFollowCamera_->SetPlayerVelocity(playerVelocity);

      playerRearFollowCamera_->ClearLandingPrediction();
      if (isAirborne && hasCameraPlanet && gravityBody && landing) {
		 LandingPrediction prediction{};
		 float predictionHorizon = std::max(
			0.0f,
			playerRearFollowCamera_->GetPreLandingPredictionHorizon());
		 float predictionGravity =
			gravityBody->useGravity ? gravityBody->gravityStrength : 0.0f;
		 if (PredictLanding(
			pos,
			playerVelocity,
			cameraPlanetCenter,
			cameraPlanetSurfaceRadius,
			landing->landingOffset,
			landing->obbHalfExtents,
			rotation,
			predictionGravity,
			predictionHorizon,
			forward,
			playerRearFollowCamera_->GetCameraRight(),
			prediction)) {
			playerRearFollowCamera_->SetLandingPrediction(
			   prediction.up,
			   prediction.backward,
			   prediction.contactPoint,
			   prediction.secondsToImpact);
		 }
      }
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

   if (landing) {
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
   json["gravityFollowCameraId"] = gravityFollowCameraId_;
   json["playerRearFollowCameraId"] = playerRearFollowCameraId_;
   json["planetLeashCameraId"] = planetLeashCameraId_;
   json["enableLandingShake"] = enableLandingShake;
   json["landingShakeAmplitude"] = landingShakeAmplitude;
   json["landingShakeFrequency"] = landingShakeFrequency;
   json["landingShakeDuration"] = landingShakeDuration;
   return json;
}

void CameraGravityBridge::Deserialize(const nlohmann::json& data) {
   if (data.contains("gravityFollowCameraId") && data.at("gravityFollowCameraId").is_string()) {
      gravityFollowCameraId_ = data.at("gravityFollowCameraId").get<std::string>();
   }
   if (data.contains("playerRearFollowCameraId") && data.at("playerRearFollowCameraId").is_string()) {
      playerRearFollowCameraId_ = data.at("playerRearFollowCameraId").get<std::string>();
   }
   if (data.contains("planetLeashCameraId") && data.at("planetLeashCameraId").is_string()) {
      planetLeashCameraId_ = data.at("planetLeashCameraId").get<std::string>();
   }
   if (data.contains("enableLandingShake")) { enableLandingShake = data["enableLandingShake"]; }
   if (data.contains("landingShakeAmplitude")) { landingShakeAmplitude = data["landingShakeAmplitude"]; }
   if (data.contains("landingShakeFrequency")) { landingShakeFrequency = data["landingShakeFrequency"]; }
   if (data.contains("landingShakeDuration")) { landingShakeDuration = data["landingShakeDuration"]; }
}

} // namespace App
