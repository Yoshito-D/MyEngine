#include "RearCameraAimSolver.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraAimSolver::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& transition = *owner_->GetComponent<RearCameraTransition>();
   const auto& gravity = *owner_->GetComponent<RearCameraGravityUp>();
   const auto& direction = *owner_->GetComponent<RearCameraDirectionTracker>();
   const auto eye = state.transform.translation;
   ApplyLookAt(camera->GetInput(), *camera, transition, direction.GetState(), state, eye,
      gravity.GetState().currentGravityUp, deltaTime);
}

/// @brief LookAt に使う注視点を計算する
/// @details 地上ではプレイヤー中心より少し上を狙い、画面内の地面比率を下げる。
///          空中ではプレイヤー中心へ戻し、着地予測中だけ接触地点の少し先へ注視点を寄せる。
GameEngine::Vector3 RearCameraAimSolver::ComputeLookTarget(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransition& transition,
   const RearCameraDirectionState& direction,
   const GameEngine::Vector3& up) const {
   // 構図専用ブレンドを使い、距離やFOVの空中補間速度から独立して
   // 地上下側と空中中央の切り替え時間を調整できるようにする。
   float targetHeight = settings.groundedTargetHeight * (1.0f - transition.GetState().currentPlayerFramingBlend);
   GameEngine::Vector3 lookTarget = input.pivotTarget + up * targetHeight;

   float preLandingGuideBlend = transition.ComputePreLandingGuideBlend(input);
   if (!input.isAirborne && transition.GetState().isLandingReleaseActive && preLandingGuideBlend > 1e-4f) {
	  // 接地時に表示していた注視点をピボット相対で保持する。
	  // ワールド着地点を保持しないため、地上移動しても古い地点へカメラが引かれない。
	  GameEngine::Vector3 releaseTarget = input.pivotTarget + transition.GetState().landingReleaseLookOffset;
	  lookTarget =
		 lookTarget + (releaseTarget - lookTarget) * preLandingGuideBlend;
   } else if (input.isAirborne && transition.GetState().currentPreLandingBlend > 1e-4f) {
	  GameEngine::Vector3 landingForward = -NormalizeOrFallback(
		 input.predictedLandingBackward,
		 -direction.currentBackward);
	  GameEngine::Vector3 terrainTarget =
		 input.predictedLandingContact
		 + landingForward * std::max(0.0f, settings.preLandingTerrainLookAhead);
	  float terrainBlend =
		 std::clamp(settings.preLandingTerrainLookBlend, 0.0f, 1.0f)
		 * transition.GetState().currentPreLandingBlend;
	  lookTarget = lookTarget + (terrainTarget - lookTarget) * terrainBlend;
   }

   return lookTarget;
}

float RearCameraAimSolver::EvaluateLookAtRecoveryCurve(const RearCameraSettings& settings, float input) const {
   const float t = std::clamp(input, 0.0f, 1.0f);
   const float inverse = 1.0f - t;
   const float control1 = std::clamp(settings.lookAtRecoveryCurveControl1, 0.0f, 1.0f);
   const float control2 = std::clamp(settings.lookAtRecoveryCurveControl2, 0.0f, 1.0f);
   const float value =
      3.0f * inverse * inverse * t * control1
      + 3.0f * inverse * t * t * control2
      + t * t * t;
   return std::clamp(value, 0.0f, 1.0f);
}

/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
/// @details LookAt の up には補間済み重力Upを使用する。
///          cachedRight / cachedUp は外部（UI など）で参照されるため確定させる。
void RearCameraAimSolver::ApplyLookAt(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransition& transition,
   const RearCameraDirectionState& direction,
   GameEngine::CameraState& state,
   const GameEngine::Vector3& eye,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   Vector3 lookTarget = ComputeLookTarget(input, settings, transition, direction, up);
   state_.lastLookTargetOffset = lookTarget - input.pivotTarget;
   Vector3 requestedUp = NormalizeOrFallback(up, state_.cachedUp);

   // zaxis = 注視点を向く方向（カメラ前方）
   Vector3 zaxis = lookTarget - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) {
	  zaxis = zaxis * (1.0f / zLen);
   } else {
      zaxis = -direction.currentBackward; // eye が pivot と一致する極端ケース
   }

   // 前フレームのRightを現在の視線平面へ平行移動し、特異点を跨いでも方位を維持する。
   Vector3 previousRight = state_.cachedRight - zaxis * zaxis.Dot(state_.cachedRight);
   float previousRightLen = previousRight.Length();
   if (previousRightLen > 1e-6f) {
      previousRight = previousRight * (1.0f / previousRightLen);
   } else {
      Vector3 fallbackAxis = (std::abs(zaxis.x) < 0.9f)
         ? Vector3{ 1.0f, 0.0f, 0.0f }
         : Vector3{ 0.0f, 1.0f, 0.0f };
      previousRight = fallbackAxis - zaxis * zaxis.Dot(fallbackAxis);
      previousRight = NormalizeOrFallback(previousRight, { 1.0f, 0.0f, 0.0f });
   }

   // gravity Upから求めたRightへ戻す量は退化度に応じて連続化する。
   // これにより、垂直落下から着地してcrossが復活した瞬間の180度ロールを防ぐ。
   Vector3 candidateRight = requestedUp.Cross(zaxis);
   float candidateRightLen = candidateRight.Length();
   state_.lastLookAtCandidateRightLength = candidateRightLen;
   state_.lastLookAtRecoveryInput = std::clamp(
      candidateRightLen / std::max(settings.lookAtRecoveryRange, 1e-4f),
      0.0f,
      1.0f);
   state_.lastLookAtRecoveryBlend = EvaluateLookAtRecoveryCurve(settings, state_.lastLookAtRecoveryInput);
   if (candidateRightLen > 1e-6f) {
      candidateRight = candidateRight * (1.0f / candidateRightLen);
   }

   Vector3 targetRight = previousRight;
   if (candidateRightLen > 1e-6f) {
      float signedRoll = SignedAngleAroundAxis(previousRight, candidateRight, zaxis, 1.0f);
      targetRight = RotateAroundAxisUnit(
         previousRight,
         zaxis,
         signedRoll * state_.lastLookAtRecoveryBlend);
   }

   targetRight = NormalizeOrFallback(targetRight - zaxis * zaxis.Dot(targetRight), previousRight);
   Vector3 targetUp = NormalizeOrFallback(zaxis.Cross(targetRight), requestedUp);
   Quaternion targetRotation = MakeCameraRotationFromBasis(targetRight, targetUp, zaxis);

   if (!state_.isViewRotationInitialized) {
      state_.currentViewRotation = targetRotation;
      state_.isViewRotationInitialized = true;
   } else {
      float rotationT = ExpSmoothingFactor(settings.rotationLerpSpeed, deltaTime);
      state_.currentViewRotation = Quaternion::Slerp(state_.currentViewRotation, targetRotation, rotationT);
   }

   // 視線前方は常にプレイヤーへ向けたまま、補間済み回転のRightを再直交化する。
   // 全回転をそのまま使って注視点を遅らせず、ロールだけを時間補間できる。
   Vector3 smoothedRight = RotateVector({ 1.0f, 0.0f, 0.0f }, state_.currentViewRotation);
   Vector3 xaxis = smoothedRight - zaxis * zaxis.Dot(smoothedRight);
   xaxis = NormalizeOrFallback(xaxis, previousRight);
   Vector3 yaxis = NormalizeOrFallback(zaxis.Cross(xaxis), requestedUp);
   state_.currentViewRotation = MakeCameraRotationFromBasis(xaxis, yaxis, zaxis);

   // キャッシュ更新
   state_.cachedRight = xaxis;
   state_.cachedUp = yaxis;
   state_.cachedForward = zaxis;

   state.transform.translation = eye;
   state.transform.SetRotationQuaternion(state_.currentViewRotation);
   Matrix4x4 view{};
   view.m[0][0] = xaxis.x;
   view.m[1][0] = xaxis.y;
   view.m[2][0] = xaxis.z;
   view.m[3][0] = -xaxis.Dot(eye);
   view.m[0][1] = yaxis.x;
   view.m[1][1] = yaxis.y;
   view.m[2][1] = yaxis.z;
   view.m[3][1] = -yaxis.Dot(eye);
   view.m[0][2] = zaxis.x;
   view.m[1][2] = zaxis.y;
   view.m[2][2] = zaxis.z;
   view.m[3][2] = -zaxis.Dot(eye);
   view.m[0][3] = 0.0f;
   view.m[1][3] = 0.0f;
   view.m[2][3] = 0.0f;
   view.m[3][3] = 1.0f;
   state.SetViewMatrix(view);
}

void RearCameraAimSolver::Reset(const RearCameraSettings& settings) {
   state_.lastLookTargetOffset = GameEngine::Vector3{ 0.0f, settings.groundedTargetHeight, 0.0f };
   state_.cachedRight = { 1.0f, 0.0f, 0.0f };
   state_.cachedUp = { 0.0f, 1.0f, 0.0f };
   state_.cachedForward = { 0.0f, 0.0f, 1.0f };
   state_.lastLookAtCandidateRightLength = 1.0f;
   state_.lastLookAtRecoveryInput = 1.0f;
   state_.lastLookAtRecoveryBlend = 1.0f;
   state_.currentViewRotation = GameEngine::Quaternion::Identity();
   state_.isViewRotationInitialized = false;
}

} // namespace App
