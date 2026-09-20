#include "RearCameraDirectionTracker.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraDirectionTracker::MutateCameraState(GameEngine::CameraState&, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& transition = *owner_->GetComponent<RearCameraTransition>();
   const auto& gravity = *owner_->GetComponent<RearCameraGravityUp>();
   const auto& aim = *owner_->GetComponent<RearCameraAimSolver>();
   const auto& planet = *owner_->GetComponent<RearCameraPlanetGuide>();
   BeginLanding(transition.GetEvents().justLanded);
   UpdateBackwardVector(camera->GetInput(), *camera, transition.GetState(), aim.GetState(), planet,
      gravity.GetState().currentGravityUp, deltaTime);
}

void RearCameraDirectionTracker::TransportWithGravity(
   const GameEngine::Vector3& oldUp,
   const GameEngine::Vector3& newUp) {
   using namespace GameEngine;
   // ─────────────────────────────────────────────────────────────
   // 【重要】今フレームの Up デルタ回転を state_.currentBackward にも適用する
   //
   // なぜこれが必要か：
   //   Up が変化しても state_.currentBackward を「重力平面へ投影」するだけでは
   //   "up.Cross(zaxis)" の符号が Up の通過点で反転し、
   //   カメラのロールが瞬間に跳ぶ。
   //
   //   Up の角変化（oldUp → newUp）と同じ回転を
   //   state_.currentBackward に掛け合わせることで、カメラ全体が剛体のように
   //   回転し、right の符号が反転しない。
   // ─────────────────────────────────────────────────────────────
   float cosAngle = std::clamp(oldUp.Dot(newUp), -1.0f, 1.0f);
   if (cosAngle < 0.9999f) {
	  Vector3 rotAxis = oldUp.Cross(newUp);
	  float axisLen = rotAxis.Length();
	  if (axisLen > 1e-6f) {
		 rotAxis = rotAxis * (1.0f / axisLen);
		 float deltaAngle = std::acos(cosAngle);

		 // ロドリゲス回転
		 float cs = std::cos(deltaAngle);
		 float sn = std::sin(deltaAngle);
		 state_.currentBackward = state_.currentBackward * cs
			+ rotAxis.Cross(state_.currentBackward) * sn
			+ rotAxis * (rotAxis.Dot(state_.currentBackward) * (1.0f - cs));
		 float bLen = state_.currentBackward.Length();
		 if (bLen > 1e-6f) state_.currentBackward = state_.currentBackward * (1.0f / bLen);
	  }
   }

}

/// @brief currentBackward を更新する
/// @details 地上では重力平面上のプレイヤー後方、空中ではプレイヤー速度の反対方向へ追従する。
/// @param up 正規化済み補間済み重力Up
/// @param deltaTime フレーム時間
void RearCameraDirectionTracker::UpdateBackwardVector(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const RearCameraAimState& aim,
   const RearCameraPlanetGuide& planet,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   Vector3 airborneFallbackBackward = NormalizeOrFallback(-input.airborneMoveForward, state_.currentBackward);
   Vector3 airborneBackward = NormalizeOrFallback(-input.playerVelocity, airborneFallbackBackward);

   // 地上後方も空中フラグに関係なく計算し、transition.currentAirborneBlend で速度後方との間を連続化する。
   // boolの切替フレームで目標方向そのものを交換しないため、補間開始のタイミングが画面へ出にくい。
   Vector3 motionBackward = ProjectOnPlaneNorm(-input.airborneMoveForward, up, state_.currentBackward);
   Vector3 displayedBackward = ProjectOnPlaneNorm(up.Cross(aim.cachedRight), up, motionBackward);
   Vector3 followBackward = NormalizeOrFallback(-input.followForward, displayedBackward);
   Vector3 projectedFollow = followBackward - up * up.Dot(followBackward);
   float projectedLength = projectedFollow.Length();
   Vector3 groundedBackward = displayedBackward;

   if (projectedLength > 1e-5f) {
	  projectedFollow = projectedFollow * (1.0f / projectedLength);
	  float projectionBlend = SmoothStep01(projectedLength / kGroundDirectionProjectionBlendRange);
	  float signedAngle = SignedAngleAroundAxis(displayedBackward, projectedFollow, up, 1.0f);
	  groundedBackward = RotateAroundAxisUnit(displayedBackward, up, signedAngle * projectionBlend);
   }

   Vector3 normalBackward = BlendUnitDirectionSafely(
	  groundedBackward,
	  airborneBackward,
	  transition.currentAirborneBlend,
	  groundedBackward,
	  up);

   Vector3 desiredBackward = normalBackward;

   // 惑星ガイドは速度後方を補助する低優先度の入力として、最終方向を追従させる前に合成する。
   // 着地前補間が始まった後は地表の接線後方を優先し、惑星ガイドを段階的に解除する。
   if (input.isAirborne) {
	  float planetDirectionBlend =
		 planet.ComputePlanetDirectionBlend(input, settings, transition, state_.currentBackward) * (1.0f - transition.currentPreLandingBlend);
	  if (planetDirectionBlend > 1e-4f) {
		 desiredBackward = BlendUnitDirectionSafely(
			desiredBackward,
			planet.GetState().currentPlanetBackward,
			planetDirectionBlend,
			desiredBackward,
			up);
	  }
   }

   // 接触前だけ予測着地速度の接線成分から求めた後方へ寄せる。
   // 接地後は現在方向を開始点に既存の角度補間で地上後方へ戻し、予測値を参照しない。
   if (input.isAirborne && transition.currentPreLandingBlend > 1e-4f) {
	  desiredBackward = BlendUnitDirectionSafely(
		 desiredBackward,
		 input.predictedLandingBackward,
		 transition.currentPreLandingBlend,
		 desiredBackward,
		 input.predictedLandingUp);
   }

   if (!state_.isInitialized) {
	  // 初回はスムーズ開始のためそのまま採用
	  state_.currentBackward = desiredBackward;
	  state_.isInitialized = true;
	  return;
   }

   if (input.isAirborne) {
      // 空中では速度後方をそのまま使えるよう、重力平面へ押し戻さない。
      state_.currentBackward = NormalizeOrFallback(state_.currentBackward, desiredBackward);
   } else {
      // 着地直後の空中後方を先に水平投影すると、垂直落下時に方位が一度で地上側へ飛ぶ。
      // 現在の3D方向を保持し、下の角度補間そのものに地上復帰を任せる。
      state_.currentBackward = NormalizeOrFallback(state_.currentBackward, desiredBackward);
   }

   float followSpeed = settings.rearLerpSpeed;
   if (input.isAirborne) {
	  followSpeed = settings.airborneForwardLerpSpeed;
	  state_.lastAirborneRearFollowSpeed = followSpeed;
	  state_.landingRearLerpStartSpeed = followSpeed;
	  state_.landingRearLerpElapsed = std::max(0.0f, settings.landingRearLerpRampSeconds);
   } else {
	  float rampSeconds = std::max(0.0f, settings.landingRearLerpRampSeconds);
	  if (rampSeconds > 1e-4f && state_.landingRearLerpElapsed < rampSeconds) {
		 // 着地した瞬間に settings.rearLerpSpeed をそのまま使うと、地上後方へ戻る力が急に強くなり画が跳ねる。
		 // 着地直前の空中追従速度から設定値へ指定秒数で近づけ、接地直後の戻り方をなめらかにする。
		 state_.landingRearLerpElapsed = std::min(state_.landingRearLerpElapsed + std::max(0.0f, deltaTime), rampSeconds);
		 float rampAlpha = std::clamp(state_.landingRearLerpElapsed / rampSeconds, 0.0f, 1.0f);
		 followSpeed = state_.landingRearLerpStartSpeed + (settings.rearLerpSpeed - state_.landingRearLerpStartSpeed) * rampAlpha;
	  }
   }
   float t = ExpSmoothingFactor(followSpeed, deltaTime);

   // 原因: 空中で速度方向が急に反対側へ変わると、state_.currentBackward と desiredBackward が
   //       180°近く離れた状態で Lerp され、補間途中がゼロに近づいて前後反転が発生する。
   // 修正: 補間率 t を角度量へ変換し、RotateTowardsUnit で一方向に回すことで反転を防ぐ。
   Vector3 trackedBackward = BlendUnitDirectionSafely(
	  state_.currentBackward,
	  desiredBackward,
	  t,
	  desiredBackward,
	  up);
   state_.currentBackward = trackedBackward;
   if (!input.isAirborne && state_.currentBackward.Dot(desiredBackward) > 0.9999f) {
      // 収束後だけ厳密な地上後方へ確定し、長時間の数値誤差を残さない。
      state_.currentBackward = desiredBackward;
   }
}

void RearCameraDirectionTracker::BeginLanding(bool justLanded) {
   // 着地した瞬間から地上用 settings.rearLerpSpeed を使うと後方復帰が急に強くなるため、
   // 着地直前の空中追従速度を始点として、指定秒数で地上設定値へ近づける。
   if (justLanded) {
	  state_.landingRearLerpElapsed = 0.0f;
	  state_.landingRearLerpStartSpeed = state_.lastAirborneRearFollowSpeed;
   }

}

void RearCameraDirectionTracker::Reset(const RearCameraSettings& settings) {
   state_.currentBackward = { 0.0f, 0.0f, -1.0f };
   state_.isInitialized = false;
   state_.landingRearLerpElapsed = std::max(0.0f, settings.landingRearLerpRampSeconds);
   state_.landingRearLerpStartSpeed = settings.airborneForwardLerpSpeed;
   state_.lastAirborneRearFollowSpeed = settings.airborneForwardLerpSpeed;
}

} // namespace App
