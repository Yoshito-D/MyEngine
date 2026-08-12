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
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {
constexpr float kLandingPredictionStepSeconds = 1.0f / 60.0f;
constexpr float kLandingPredictionDirectionEpsilon = 1e-4f;
constexpr float kPresentationBackwardReversalDot = -0.2f;
constexpr float kPresentationCameraForwardGravityAxisDot = 0.85f;
constexpr const char* kPresentationDebugCaptureDirectory =
   "../Generated/PresentationScreenshots/20260809_camera_debug";
constexpr const char* kPresentationCleanCaptureDirectory =
   "../Generated/PresentationScreenshots/20260809_camera_clean";

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

void RequestPresentationScreenshot(const char* filename, bool includeDebugGuides) {
   const char* captureDirectory = includeDebugGuides
      ? kPresentationDebugCaptureDirectory
      : kPresentationCleanCaptureDirectory;
   GameEngine::EngineContext::RequestScreenshot(
      std::filesystem::path(captureDirectory) / filename);
}

struct LandingPrediction {
   GameEngine::Vector3 up = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 backward = { 0.0f, 0.0f, -1.0f };
   GameEngine::Vector3 contactPoint = { 0.0f, 0.0f, 0.0f };
   float secondsToImpact = 0.0f;
   std::vector<GameEngine::Vector3> trajectoryPoints;
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
   outPrediction.trajectoryPoints.clear();
   outPrediction.trajectoryPoints.push_back(position);
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
	  outPrediction.trajectoryPoints.push_back(simulatedPosition);

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
		 outPrediction.trajectoryPoints.back() = hitPosition;
		 return true;
	  }

	  previousClearance = nextClearance;
	  elapsed += step;
   }

   return false;
}

void DrawDebugArrow(
   const GameEngine::Vector3& origin,
   const GameEngine::Vector3& direction,
   float length,
   const GameEngine::Vector4& color) {
   const float safeLength = std::max(0.25f, length);
   const GameEngine::Vector3 normalizedDirection = NormalizeOrFallback(
      direction,
      { 0.0f, 1.0f, 0.0f });
   const GameEngine::Vector3 tip = origin + normalizedDirection * safeLength;
   GameEngine::EngineContext::DrawLine(origin, tip, color, false);
   GameEngine::EngineContext::DrawCone(
      tip,
      safeLength * 0.08f,
      safeLength * 0.22f,
      -normalizedDirection,
      color,
      false);
}

void DrawPresentationLegendEntry(
   const char* text,
   float y,
   const GameEngine::Vector4& color) {
   GameEngine::TextStyle shadowStyle{};
   shadowStyle.fontId = "851Gkktt";
   shadowStyle.fontSize = 28;
   shadowStyle.color = { 0.0f, 0.0f, 0.0f, 0.9f };
   shadowStyle.sortingOrder = 899;
   GameEngine::EngineContext::DrawUIText(text, { 26.0f, y + 2.0f }, shadowStyle);

   GameEngine::TextStyle style = shadowStyle;
   style.color = color;
   style.sortingOrder = 900;
   GameEngine::EngineContext::DrawUIText(text, { 24.0f, y }, style);
}

void DrawPresentationGuides(
   const GameEngine::Vector3& playerPosition,
   const GameEngine::Vector3& motionBackward,
   const GameEngine::Vector3& planetGuideBackward,
   const GameEngine::Vector3& targetGravityUp,
   const GameEngine::Vector3& cameraForward,
   const GameEngine::Vector3& cameraRight,
   const LandingPrediction* prediction,
   float guideLength,
   int conditionPreviewMode) {
   const GameEngine::Vector4 trajectoryColor = { 1.0f, 0.35f, 0.05f, 1.0f };
   const GameEngine::Vector4 contactColor = { 0.1f, 0.95f, 1.0f, 1.0f };
   const GameEngine::Vector4 landingUpColor = { 0.15f, 1.0f, 0.25f, 1.0f };
   const GameEngine::Vector4 targetUpColor = { 0.1f, 0.45f, 1.0f, 1.0f };
   const GameEngine::Vector4 cameraForwardColor = { 1.0f, 0.1f, 0.1f, 1.0f };
   const GameEngine::Vector4 cameraBasisColor = { 1.0f, 0.2f, 1.0f, 1.0f };
   const float safeGuideLength = std::max(1.0f, guideLength);

   const GameEngine::Vector3 safeRight = NormalizeOrFallback(
      cameraRight,
      { 1.0f, 0.0f, 0.0f });
   const GameEngine::Vector3 safeTargetUp = NormalizeOrFallback(
      targetGravityUp,
      { 0.0f, 1.0f, 0.0f });
   const GameEngine::Vector3 safeMotionBackward = NormalizeOrFallback(
      motionBackward,
      -safeTargetUp);
   const GameEngine::Vector3 safePlanetGuideBackward = NormalizeOrFallback(
      planetGuideBackward,
      safeTargetUp);
   const GameEngine::Vector3 safeCameraForward = NormalizeOrFallback(
      cameraForward,
      -safeMotionBackward);
   if (conditionPreviewMode == 1) {
      const GameEngine::Vector3 guideOrigin =
         playerPosition + safeTargetUp * safeGuideLength * 0.45f;
      DrawDebugArrow(
         guideOrigin - safeRight * 1.5f,
         safeMotionBackward,
         safeGuideLength * 0.5f,
         targetUpColor);
      DrawDebugArrow(
         guideOrigin + safeRight * 1.5f,
         safePlanetGuideBackward,
         safeGuideLength * 0.5f,
         cameraBasisColor);
      const float backwardDot = std::clamp(
         safeMotionBackward.Dot(safePlanetGuideBackward),
         -1.0f,
         1.0f);
      const float angleDegrees = std::acos(backwardDot) * 57.2957795f;
      char conditionText[128]{};
      std::snprintf(
         conditionText,
         sizeof(conditionText),
         "実測: 速度後方と惑星ガイドの差=%.0f度 (dot=%.2f)",
         angleDegrees,
         backwardDot);
      DrawPresentationLegendEntry("青: 速度後方（実測）", 24.0f, targetUpColor);
      DrawPresentationLegendEntry("紫: 惑星ガイド後方（実測）", 60.0f, cameraBasisColor);
      DrawPresentationLegendEntry(
         conditionText,
         104.0f,
         { 1.0f, 1.0f, 1.0f, 1.0f });
      return;
   }
   if (conditionPreviewMode == 2) {
      const GameEngine::Vector3 gravityDown = -safeTargetUp;
      const GameEngine::Vector3 guideOrigin =
         playerPosition + safeTargetUp * safeGuideLength * 0.75f;
      DrawDebugArrow(
         guideOrigin - safeRight * 1.5f,
         gravityDown,
         safeGuideLength * 0.65f,
         targetUpColor);
      DrawDebugArrow(
         guideOrigin + safeRight * 1.5f,
         safeCameraForward,
         safeGuideLength * 0.65f,
         cameraForwardColor);
      char conditionText[112]{};
      std::snprintf(
         conditionText,
         sizeof(conditionText),
         "実測: 前方と重力軸 |dot|=%.2f",
         std::abs(safeCameraForward.Dot(safeTargetUp)));
      DrawPresentationLegendEntry("青: 重力方向（実測）", 24.0f, targetUpColor);
      DrawPresentationLegendEntry("赤: カメラ前方（実測）", 60.0f, cameraForwardColor);
      DrawPresentationLegendEntry(
         conditionText,
         104.0f,
         { 1.0f, 1.0f, 1.0f, 1.0f });
      return;
   }

   if (!prediction) {
      return;
   }

   const std::vector<GameEngine::Vector3>& points = prediction->trajectoryPoints;
   for (size_t index = 1; index < points.size(); ++index) {
      GameEngine::EngineContext::DrawLine(
         points[index - 1],
         points[index],
         trajectoryColor,
         false);
   }

   const float trajectoryPointRadius = safeGuideLength * 0.025f;
   for (size_t index = 0; index < points.size(); index += 8) {
      GameEngine::EngineContext::DrawSphere(
         points[index],
         trajectoryPointRadius,
         trajectoryColor,
         false);
   }

   const GameEngine::Vector3 landingUp = NormalizeOrFallback(
      prediction->up,
      targetGravityUp);
   const GameEngine::Vector3 markerCenter =
      prediction->contactPoint + landingUp * safeGuideLength * 0.04f;
   GameEngine::EngineContext::DrawSphere(
      markerCenter,
      safeGuideLength * 0.13f,
      contactColor,
      false);
   GameEngine::EngineContext::DrawCircle(
      markerCenter,
      safeGuideLength * 0.28f,
      landingUp,
      contactColor,
      false);
   DrawDebugArrow(
      markerCenter,
      landingUp,
      safeGuideLength * 0.25f,
      landingUpColor);

   DrawPresentationLegendEntry("橙: 予測軌道", 24.0f, trajectoryColor);
   DrawPresentationLegendEntry("水色: 接触予定点", 60.0f, contactColor);
   DrawPresentationLegendEntry("緑: 着地後Up", 96.0f, landingUpColor);
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
   presentationCaptureElapsed_ = 0.0f;
   presentationJumpTriggered_ = false;
   presentationGroundCaptured_ = false;
   presentationPredictionCaptured_ = false;
   presentationBeforeContactCaptured_ = false;
   presentationContactCaptured_ = false;
   presentationAfterLandingCaptured_ = false;
   presentationAfterLandingElapsed_ = 0.0f;
   presentationStraightDownCaptured_ = false;
   presentationBackwardReversalCaptured_ = false;
   presentationVideoFrameAccumulator_ = 0.0f;
   presentationVideoFrameIndex_ = 0;

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

void CameraGravityBridge::Update(float deltaTime) {
   // オーナー不在時は更新しない
   if (!HasOwner()) { return; }

   // ワールド座標の取得元（Transform）を参照
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* landing = GetOwner().GetComponent<CharacterLanding>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();
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
      if (jump) {
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
      LandingPrediction prediction{};
      bool hasLandingPrediction = false;
      if (isAirborne && hasCameraPlanet && gravityBody && landing) {
		 float predictionHorizon = std::max(
			0.0f,
			playerRearFollowCamera_->GetPreLandingPredictionHorizon());
		 float predictionGravity =
			gravityBody->useGravity ? gravityBody->gravityStrength : 0.0f;
		 hasLandingPrediction = PredictLanding(
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
			prediction);
		 if (hasLandingPrediction) {
			playerRearFollowCamera_->SetLandingPrediction(
			   prediction.up,
			   prediction.backward,
			   prediction.contactPoint,
			   prediction.secondsToImpact);
		 }
      }

      const GameEngine::Vector3 actualMotionBackward = NormalizeOrFallback(
         -playerVelocity,
         -forward);
      const GameEngine::Vector3 actualPlanetGuideBackward = NormalizeOrFallback(
         pos - cameraPlanetCenter,
         gravityUp);
      const float backwardReversalDot =
         actualMotionBackward.Dot(actualPlanetGuideBackward);

      if (debugDrawPresentationGuides) {
         int conditionPreviewMode = 0;
         const GameEngine::Vector3 actualCameraForward = NormalizeOrFallback(
            playerRearFollowCamera_->GetCameraForward(),
            forward);
         const float cameraForwardGravityAxisDot =
            std::abs(actualCameraForward.Dot(gravityUp));
         if (isAirborne
            && playerVelocity.Length() > 0.5f
            && backwardReversalDot <= kPresentationBackwardReversalDot) {
            conditionPreviewMode = 1;
         } else if (isAirborne
            && cameraForwardGravityAxisDot
               >= kPresentationCameraForwardGravityAxisDot) {
            conditionPreviewMode = 2;
         }
         DrawPresentationGuides(
            pos,
            actualMotionBackward,
            actualPlanetGuideBackward,
            gravityUp,
            actualCameraForward,
            playerRearFollowCamera_->GetCameraRight(),
            hasLandingPrediction ? &prediction : nullptr,
            presentationGuideLength,
            conditionPreviewMode);
      }

      if (autoCapturePresentationSequence) {
#ifdef USE_IMGUI
         GameEngine::EngineContext::SetDockSpaceVisible(false);
#endif
         presentationCaptureElapsed_ += std::max(0.0f, deltaTime);
         if (autoCapturePresentationVideoFrames) {
            const float frameRate = std::max(1.0f, presentationVideoFrameRate);
            const float captureStart = std::max(
               0.0f,
               presentationVideoCaptureStartDelay);
            const float captureDuration = std::max(
               0.0f,
               presentationVideoCaptureDuration);
            const uint32_t maxFrameCount = static_cast<uint32_t>(std::ceil(
               captureDuration * frameRate));
            if (presentationCaptureElapsed_ >= captureStart
               && presentationCaptureElapsed_ <= captureStart + captureDuration
               && presentationVideoFrameIndex_ < maxFrameCount) {
               const float frameInterval = 1.0f / frameRate;
               presentationVideoFrameAccumulator_ += std::max(0.0f, deltaTime);
               if (presentationVideoFrameIndex_ == 0
                  || presentationVideoFrameAccumulator_ >= frameInterval) {
                  char filename[96]{};
                  std::snprintf(
                     filename,
                     sizeof(filename),
                     "video_frames/frame_%04u.png",
                     presentationVideoFrameIndex_);
                  RequestPresentationScreenshot(filename, debugDrawPresentationGuides);
                  ++presentationVideoFrameIndex_;
                  presentationVideoFrameAccumulator_ = std::fmod(
                     presentationVideoFrameAccumulator_,
                     frameInterval);
               }
            }
         }
         const float groundCaptureTime = std::max(
            0.75f,
            presentationCaptureJumpDelay - 0.5f);
         if (!presentationGroundCaptured_ && presentationCaptureElapsed_ >= groundCaptureTime) {
            RequestPresentationScreenshot(
               "01_direction_axes_grounded.png",
               debugDrawPresentationGuides);
            presentationGroundCaptured_ = true;
         }

         if (!presentationJumpTriggered_
            && presentationCaptureElapsed_ >= std::max(0.0f, presentationCaptureJumpDelay)
            && jump
            && !isAirborne) {
            jump->Jump(gravityUp);
            presentationJumpTriggered_ = true;
         }

         if (hasLandingPrediction && !presentationPredictionCaptured_) {
            RequestPresentationScreenshot(
               "02_prediction_start.png",
               debugDrawPresentationGuides);
            presentationPredictionCaptured_ = true;
         }
         if (hasLandingPrediction
            && !presentationBeforeContactCaptured_
            && prediction.secondsToImpact <= 0.18f) {
            RequestPresentationScreenshot(
               "03_contact_before.png",
               debugDrawPresentationGuides);
            presentationBeforeContactCaptured_ = true;
         }

         const GameEngine::Vector3 cameraForward = NormalizeOrFallback(
            playerRearFollowCamera_->GetCameraForward(),
            forward);
         if (isAirborne
            && !presentationBackwardReversalCaptured_
            && playerVelocity.Length() > 0.5f
            && backwardReversalDot <= kPresentationBackwardReversalDot) {
            RequestPresentationScreenshot(
               "condition_backward_reversal.png",
               debugDrawPresentationGuides);
            presentationBackwardReversalCaptured_ = true;
         }
         if (isAirborne
            && !presentationStraightDownCaptured_
            && std::abs(cameraForward.Dot(gravityUp))
               >= kPresentationCameraForwardGravityAxisDot) {
            RequestPresentationScreenshot(
               "condition_straight_down.png",
               debugDrawPresentationGuides);
            presentationStraightDownCaptured_ = true;
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
      if (autoCapturePresentationSequence && presentationJumpTriggered_) {
         if (isGrounded && !wasGrounded_ && !presentationContactCaptured_) {
            RequestPresentationScreenshot("04_contact.png", debugDrawPresentationGuides);
            presentationContactCaptured_ = true;
            presentationAfterLandingElapsed_ = 0.0f;
         } else if (presentationContactCaptured_ && !presentationAfterLandingCaptured_) {
            presentationAfterLandingElapsed_ += std::max(0.0f, deltaTime);
            if (presentationAfterLandingElapsed_ >= 0.45f) {
               RequestPresentationScreenshot(
                  "05_after_landing.png",
                  debugDrawPresentationGuides);
               presentationAfterLandingCaptured_ = true;
            }
         }
      }
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
   ImGui::Separator();
   ImGui::Checkbox(
      Tr("発表用カメラガイド", "Presentation Camera Guides"),
      &debugDrawPresentationGuides);
   ImGui::DragFloat(
      Tr("ガイド矢印の長さ", "Guide Arrow Length"),
      &presentationGuideLength,
      0.1f,
      1.0f,
      30.0f);
   ImGui::Checkbox(
      Tr("発表用連続撮影", "Auto-capture Presentation Sequence"),
      &autoCapturePresentationSequence);
   ImGui::DragFloat(
      Tr("自動ジャンプ待ち時間", "Auto-jump Delay"),
      &presentationCaptureJumpDelay,
      0.05f,
      0.0f,
      10.0f);
   ImGui::Checkbox(
      Tr("発表動画用の連番PNG", "Presentation Video Frames"),
      &autoCapturePresentationVideoFrames);
   ImGui::DragFloat(
      Tr("動画フレームレート", "Video Frame Rate"),
      &presentationVideoFrameRate,
      1.0f,
      1.0f,
      60.0f);
   ImGui::DragFloat(
      Tr("動画撮影開始待ち", "Video Capture Start Delay"),
      &presentationVideoCaptureStartDelay,
      0.05f,
      0.0f,
      30.0f);
   ImGui::DragFloat(
      Tr("動画撮影時間", "Video Capture Duration"),
      &presentationVideoCaptureDuration,
      0.1f,
      0.0f,
      60.0f);
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
   json["debugDrawPresentationGuides"] = debugDrawPresentationGuides;
   json["presentationGuideLength"] = presentationGuideLength;
   json["autoCapturePresentationSequence"] = autoCapturePresentationSequence;
   json["presentationCaptureJumpDelay"] = presentationCaptureJumpDelay;
   json["autoCapturePresentationVideoFrames"] = autoCapturePresentationVideoFrames;
   json["presentationVideoFrameRate"] = presentationVideoFrameRate;
   json["presentationVideoCaptureStartDelay"] = presentationVideoCaptureStartDelay;
   json["presentationVideoCaptureDuration"] = presentationVideoCaptureDuration;
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
   if (data.contains("debugDrawPresentationGuides") && data.at("debugDrawPresentationGuides").is_boolean()) {
      debugDrawPresentationGuides = data.at("debugDrawPresentationGuides").get<bool>();
   }
   if (data.contains("presentationGuideLength") && data.at("presentationGuideLength").is_number()) {
      presentationGuideLength = std::max(1.0f, data.at("presentationGuideLength").get<float>());
   }
   if (data.contains("autoCapturePresentationSequence") && data.at("autoCapturePresentationSequence").is_boolean()) {
      autoCapturePresentationSequence = data.at("autoCapturePresentationSequence").get<bool>();
   }
   if (data.contains("presentationCaptureJumpDelay") && data.at("presentationCaptureJumpDelay").is_number()) {
      presentationCaptureJumpDelay = std::max(0.0f, data.at("presentationCaptureJumpDelay").get<float>());
   }
   if (data.contains("autoCapturePresentationVideoFrames")
      && data.at("autoCapturePresentationVideoFrames").is_boolean()) {
      autoCapturePresentationVideoFrames =
         data.at("autoCapturePresentationVideoFrames").get<bool>();
   }
   if (data.contains("presentationVideoFrameRate")
      && data.at("presentationVideoFrameRate").is_number()) {
      presentationVideoFrameRate = std::clamp(
         data.at("presentationVideoFrameRate").get<float>(),
         1.0f,
         60.0f);
   }
   if (data.contains("presentationVideoCaptureStartDelay")
      && data.at("presentationVideoCaptureStartDelay").is_number()) {
      presentationVideoCaptureStartDelay = std::max(
         0.0f,
         data.at("presentationVideoCaptureStartDelay").get<float>());
   }
   if (data.contains("presentationVideoCaptureDuration")
      && data.at("presentationVideoCaptureDuration").is_number()) {
      presentationVideoCaptureDuration = std::max(
         0.0f,
         data.at("presentationVideoCaptureDuration").get<float>());
   }
}

} // namespace App
