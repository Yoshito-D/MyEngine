#include "GameProject/App/Component/Camera/CameraPresentationCapture.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameEngine/Framework/EngineContext.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace App {
void CameraPresentationCapture::Reset(const VehicleEventSnapshot& snapshot) {
   *this = {};
   eventCursor_.Reset(snapshot);
}

using RearCameraMath::NormalizeOrFallback;

namespace {
constexpr float kPresentationBackwardReversalDot = -0.2f;
constexpr float kPresentationCameraForwardGravityAxisDot = 0.85f;
constexpr const char* kPresentationDebugCaptureDirectory = "../Generated/PresentationScreenshots/20260809_camera_debug";
constexpr const char* kPresentationCleanCaptureDirectory = "../Generated/PresentationScreenshots/20260809_camera_clean";

void RequestPresentationScreenshot(const char* filename, bool includeDebugGuides) {
   const char* captureDirectory =
      includeDebugGuides ? kPresentationDebugCaptureDirectory : kPresentationCleanCaptureDirectory;
   GameEngine::EngineContext::RequestScreenshot(std::filesystem::path(captureDirectory) / filename);
}

void DrawDebugArrow(const GameEngine::Vector3& origin, const GameEngine::Vector3& direction, float length,
   const GameEngine::Vector4& color) {
   const float safeLength = std::max(0.25f, length);
   const GameEngine::Vector3 normalizedDirection = NormalizeOrFallback(direction, {0.0f, 1.0f, 0.0f});
   const GameEngine::Vector3 tip = origin + normalizedDirection * safeLength;
   GameEngine::EngineContext::DrawLine(origin, tip, color, false);
   GameEngine::EngineContext::DrawCone(tip, safeLength * 0.08f, safeLength * 0.22f, -normalizedDirection, color, false);
}

void DrawPresentationLegendEntry(const char* text, float y, const GameEngine::Vector4& color) {
   GameEngine::TextStyle shadowStyle{};
   shadowStyle.fontId = "arial";
   shadowStyle.fontSize = 28;
   shadowStyle.color = {0.0f, 0.0f, 0.0f, 0.9f};
   shadowStyle.sortingOrder = 899;
   GameEngine::EngineContext::DrawUIText(text, {26.0f, y + 2.0f}, shadowStyle);

   GameEngine::TextStyle style = shadowStyle;
   style.color = color;
   style.sortingOrder = 900;
   GameEngine::EngineContext::DrawUIText(text, {24.0f, y}, style);
}
}

void CameraPresentationCapture::DrawPresentationGuides(const GameEngine::Vector3& playerPosition,
   const GameEngine::Vector3& motionBackward, const GameEngine::Vector3& planetGuideBackward,
   const GameEngine::Vector3& targetGravityUp, const GameEngine::Vector3& cameraForward,
   const GameEngine::Vector3& cameraRight, const LandingPrediction* prediction, float guideLength,
   std::int32_t conditionPreviewMode) {
   const GameEngine::Vector4 trajectoryColor = {1.0f, 0.35f, 0.05f, 1.0f};
   const GameEngine::Vector4 contactColor = {0.1f, 0.95f, 1.0f, 1.0f};
   const GameEngine::Vector4 landingUpColor = {0.15f, 1.0f, 0.25f, 1.0f};
   const GameEngine::Vector4 targetUpColor = {0.1f, 0.45f, 1.0f, 1.0f};
   const GameEngine::Vector4 cameraForwardColor = {1.0f, 0.1f, 0.1f, 1.0f};
   const GameEngine::Vector4 cameraBasisColor = {1.0f, 0.2f, 1.0f, 1.0f};
   const float safeGuideLength = std::max(1.0f, guideLength);

   const GameEngine::Vector3 safeRight = NormalizeOrFallback(cameraRight, {1.0f, 0.0f, 0.0f});
   const GameEngine::Vector3 safeTargetUp = NormalizeOrFallback(targetGravityUp, {0.0f, 1.0f, 0.0f});
   const GameEngine::Vector3 safeMotionBackward = NormalizeOrFallback(motionBackward, -safeTargetUp);
   const GameEngine::Vector3 safePlanetGuideBackward = NormalizeOrFallback(planetGuideBackward, safeTargetUp);
   const GameEngine::Vector3 safeCameraForward = NormalizeOrFallback(cameraForward, -safeMotionBackward);

   if (conditionPreviewMode == 1) {
      // 後方ベクトルが反転する条件では、比較する2軸を並べて角度差を可視化する。
      const GameEngine::Vector3 guideOrigin = playerPosition + safeTargetUp * safeGuideLength * 0.45f;
      DrawDebugArrow(guideOrigin - safeRight * 1.5f, safeMotionBackward, safeGuideLength * 0.5f, targetUpColor);
      DrawDebugArrow(guideOrigin + safeRight * 1.5f, safePlanetGuideBackward, safeGuideLength * 0.5f, cameraBasisColor);
      const float backwardDot = std::clamp(safeMotionBackward.Dot(safePlanetGuideBackward), -1.0f, 1.0f);
      const float angleDegrees = std::acos(backwardDot) * 57.2957795f;
      char conditionText[128]{};
      std::snprintf(conditionText, sizeof(conditionText),
         "Measured: velocity-back vs planet-guide = %.0f deg (dot=%.2f)", angleDegrees, backwardDot);
      DrawPresentationLegendEntry("BLUE: velocity back (measured)", 24.0f, targetUpColor);
      DrawPresentationLegendEntry("PURPLE: planet guide back (measured)", 60.0f, cameraBasisColor);
      DrawPresentationLegendEntry(conditionText, 104.0f, {1.0f, 1.0f, 1.0f, 1.0f});
      return;
   }

   if (conditionPreviewMode == 2) {
      // 視線が重力軸へ寄る条件では、重なりを避けた平行配置で実測方向を示す。
      const GameEngine::Vector3 gravityDown = -safeTargetUp;
      const GameEngine::Vector3 guideOrigin = playerPosition + safeTargetUp * safeGuideLength * 0.75f;
      DrawDebugArrow(guideOrigin - safeRight * 1.5f, gravityDown, safeGuideLength * 0.65f, targetUpColor);
      DrawDebugArrow(guideOrigin + safeRight * 1.5f, safeCameraForward, safeGuideLength * 0.65f, cameraForwardColor);
      char conditionText[112]{};
      std::snprintf(conditionText, sizeof(conditionText), "Measured: forward vs gravity axis |dot|=%.2f",
         std::abs(safeCameraForward.Dot(safeTargetUp)));
      DrawPresentationLegendEntry("BLUE: gravity direction (measured)", 24.0f, targetUpColor);
      DrawPresentationLegendEntry("RED: camera forward (measured)", 60.0f, cameraForwardColor);
      DrawPresentationLegendEntry(conditionText, 104.0f, {1.0f, 1.0f, 1.0f, 1.0f});
      return;
   }

   if (!prediction) return;

   const std::vector<GameEngine::Vector3>& points = prediction->trajectoryPoints;

   // 軌道は全区間を線で結び、球マーカーだけを間引いて画面の密集を抑える。
   for (size_t index = 1; index < points.size(); ++index) {
      GameEngine::EngineContext::DrawLine(points[index - 1], points[index], trajectoryColor, false);
   }

   const float trajectoryPointRadius = safeGuideLength * 0.025f;

   for (size_t index = 0; index < points.size(); index += 8) {
      GameEngine::EngineContext::DrawSphere(points[index], trajectoryPointRadius, trajectoryColor, false);
   }

   const GameEngine::Vector3 landingUp = NormalizeOrFallback(prediction->up, targetGravityUp);
   const GameEngine::Vector3 markerCenter = prediction->contactPoint + landingUp * safeGuideLength * 0.04f;
   GameEngine::EngineContext::DrawSphere(markerCenter, safeGuideLength * 0.13f, contactColor, false);
   GameEngine::EngineContext::DrawCircle(markerCenter, safeGuideLength * 0.28f, landingUp, contactColor, false);
   DrawDebugArrow(markerCenter, landingUp, safeGuideLength * 0.25f, landingUpColor);

   DrawPresentationLegendEntry("ORANGE: predicted trajectory", 24.0f, trajectoryColor);
   DrawPresentationLegendEntry("CYAN: predicted contact", 60.0f, contactColor);
   DrawPresentationLegendEntry("GREEN: landing Up", 96.0f, landingUpColor);
}

void CameraPresentationCapture::Update(const CameraPresentationCaptureSettings& settings, float deltaTime,
   bool cameraSelected, PlayerRearFollowCamera& camera, CharacterJump* jump, const VehicleEventSnapshot& snapshot,
   const LandingPrediction* prediction) {
   const auto changes = eventCursor_.Read(snapshot);

   // 他のカメラで発生した接触を、後から選択した時点の撮影対象にしない。
   if (!cameraSelected) return;

   const auto& input = camera.GetInput();
   const auto& pos = input.pivotTarget;
   const auto& playerVelocity = input.playerVelocity;
   const auto& forward = input.followForward;
   const auto& gravityUp = input.gravityUp;
   const auto& cameraPlanetCenter = input.planetCenter;
   const bool isAirborne = input.isAirborne;
   const bool hasLandingPrediction = prediction != nullptr;
   const GameEngine::Vector3 actualMotionBackward = NormalizeOrFallback(-playerVelocity, -forward);
   const GameEngine::Vector3 actualPlanetGuideBackward = NormalizeOrFallback(pos - cameraPlanetCenter, gravityUp);
   const float backwardReversalDot = actualMotionBackward.Dot(actualPlanetGuideBackward);

   if (settings.debugDrawPresentationGuides) {
      // 反転条件を先に選び、同時成立時も発表用ガイドの意味を一意に保つ。
      std::int32_t conditionPreviewMode = 0;
      const GameEngine::Vector3 actualCameraForward = NormalizeOrFallback(camera.GetCameraForward(), forward);
      const float cameraForwardGravityAxisDot = std::abs(actualCameraForward.Dot(gravityUp));

      if (isAirborne && playerVelocity.Length() > 0.5f && backwardReversalDot <= kPresentationBackwardReversalDot) {
         conditionPreviewMode = 1;
      } else if (isAirborne && cameraForwardGravityAxisDot >= kPresentationCameraForwardGravityAxisDot) {
         conditionPreviewMode = 2;
      }

      DrawPresentationGuides(pos, actualMotionBackward, actualPlanetGuideBackward, gravityUp, actualCameraForward,
         camera.GetCameraRight(), prediction, settings.presentationGuideLength, conditionPreviewMode);
   }

   if (settings.autoCapturePresentationSequence) {
#ifdef USE_IMGUI
      // 発表素材にはエディタUIを含めず、ゲーム画面とガイドだけを記録する。
      GameEngine::EngineContext::SetDockSpaceVisible(false);
#endif
      if (!presentationMeasurementStarted_) {
         // スクリーンショットとCSVを同じ試行へ結び付け、資料上の時刻と実測値を対応させる。
         camera.StartCameraMeasurement("presentation_sequence_warmup");
         presentationMeasurementStarted_ = true;
      }

      presentationCaptureElapsed_ += std::max(0.0f, deltaTime);

      if (settings.autoCapturePresentationVideoFrames) {
         // 実フレームレートとは独立した累積時間で、指定レートの連番画像へ間引く。
         const float frameRate = std::max(1.0f, settings.presentationVideoFrameRate);
         const float captureStart = std::max(0.0f, settings.presentationVideoCaptureStartDelay);
         const float captureDuration = std::max(0.0f, settings.presentationVideoCaptureDuration);
         const uint32_t maxFrameCount = static_cast<uint32_t>(std::ceil(captureDuration * frameRate));

         if (presentationCaptureElapsed_ >= captureStart &&
             presentationCaptureElapsed_ <= captureStart + captureDuration &&
             presentationVideoFrameIndex_ < maxFrameCount) {
            const float frameInterval = 1.0f / frameRate;
            presentationVideoFrameAccumulator_ += std::max(0.0f, deltaTime);

            if (presentationVideoFrameIndex_ == 0 || presentationVideoFrameAccumulator_ >= frameInterval) {
               char filename[96]{};
               std::snprintf(filename, sizeof(filename), "video_frames/frame_%04u.png", presentationVideoFrameIndex_);
               RequestPresentationScreenshot(filename, settings.debugDrawPresentationGuides);
               ++presentationVideoFrameIndex_;

               // 超過時間を次の撮影判定へ繰り越し、長いフレーム後の位相ずれを抑える。
               presentationVideoFrameAccumulator_ = std::fmod(presentationVideoFrameAccumulator_, frameInterval);
            }
         }
      }

      const float groundCaptureTime = std::max(0.75f, settings.presentationCaptureJumpDelay - 0.5f);

      // ジャンプ前後の節目を一度ずつ記録し、同じ状態が続いても重複撮影しない。
      if (!presentationGroundCaptured_ && presentationCaptureElapsed_ >= groundCaptureTime) {
         RequestPresentationScreenshot("01_direction_axes_grounded.png", settings.debugDrawPresentationGuides);
         presentationGroundCaptured_ = true;
      }

      if (!presentationJumpTriggered_ &&
          presentationCaptureElapsed_ >= std::max(0.0f, settings.presentationCaptureJumpDelay) &&
          camera.GetCameraMeasurementSampleCount() >= 30 && jump && jump->IsEnabled() && !isAirborne) {
         // シーン読込直後の姿勢初期化を実測最大値へ混ぜず、安定した直前フレームを始点にする。
         camera.StartCameraMeasurement("presentation_sequence_current_settings");
         presentationMeasurementResetForJump_ = true;
         jump->Jump(gravityUp);
         presentationJumpTriggered_ = true;
      }

      if (hasLandingPrediction && !presentationPredictionCaptured_) {
         RequestPresentationScreenshot("02_prediction_start.png", settings.debugDrawPresentationGuides);
         presentationPredictionCaptured_ = true;
      }

      if (hasLandingPrediction && !presentationBeforeContactCaptured_ && prediction->secondsToImpact <= 0.18f) {
         RequestPresentationScreenshot("03_contact_before.png", settings.debugDrawPresentationGuides);
         presentationBeforeContactCaptured_ = true;
      }

      const GameEngine::Vector3 cameraForward = NormalizeOrFallback(camera.GetCameraForward(), forward);

      if (isAirborne && !presentationBackwardReversalCaptured_ && playerVelocity.Length() > 0.5f &&
          backwardReversalDot <= kPresentationBackwardReversalDot) {
         RequestPresentationScreenshot("condition_backward_reversal.png", settings.debugDrawPresentationGuides);
         presentationBackwardReversalCaptured_ = true;
      }

      if (isAirborne && !presentationStraightDownCaptured_ &&
          std::abs(cameraForward.Dot(gravityUp)) >= kPresentationCameraForwardGravityAxisDot) {
         RequestPresentationScreenshot("condition_straight_down.png", settings.debugDrawPresentationGuides);
         presentationStraightDownCaptured_ = true;
      }
   }
   if (settings.autoCapturePresentationSequence && presentationJumpTriggered_) {
      // 接地の立ち上がりを接触瞬間とし、その後は姿勢が落ち着くまで別タイマーで待つ。
      if (changes.landed && !presentationContactCaptured_) {
         RequestPresentationScreenshot("04_contact.png", settings.debugDrawPresentationGuides);
         presentationContactCaptured_ = true;
         presentationAfterLandingElapsed_ = 0.0f;
      } else if (presentationContactCaptured_ && !presentationAfterLandingCaptured_) {
         presentationAfterLandingElapsed_ += std::max(0.0f, deltaTime);

         if (presentationAfterLandingElapsed_ >= 0.45f) {
            RequestPresentationScreenshot("05_after_landing.png", settings.debugDrawPresentationGuides);
            presentationAfterLandingCaptured_ = true;

            if (presentationMeasurementStarted_ && presentationMeasurementResetForJump_ &&
                !presentationMeasurementSaved_) {
               presentationMeasurementSaved_ = camera.StopCameraMeasurement(true);
            }
         }
      }
   }

   if (settings.autoCapturePresentationSequence && presentationMeasurementStarted_ && !presentationMeasurementSaved_ &&
       presentationCaptureElapsed_ >= 30.0f) {
      // 着地条件が成立しない試行でも無限に記録せず、失敗を含む30秒分を検証材料として残す。
      presentationMeasurementSaved_ = camera.StopCameraMeasurement(true);
   }
}
}
