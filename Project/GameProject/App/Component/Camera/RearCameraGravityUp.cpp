#include "RearCameraGravityUp.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraDirectionTracker.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraGravityUp::MutateCameraState(GameEngine::CameraState&, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& transition = *owner_->GetComponent<RearCameraTransition>();
   const auto& aim = *owner_->GetComponent<RearCameraAimSolver>();
   auto& direction = *owner_->GetComponent<RearCameraDirectionTracker>();
   SmoothGravityUp(camera->GetInput(), *camera, transition.GetState(), aim.GetState(), direction, deltaTime);
}

/// @brief 目標重力Up に向けて currentGravityUp を角速度制限付きで回転する
/// @details 惑星切り替え時に gravityUp（目標）が急変しても、
///          gravityUpLerpSpeed * deltaTime を1フレームの最大回転量として
///          currentGravityUp を少しずつ近づけ、カメラの急なRoll変化を抑制する。
///          同じフレームのUpデルタ回転は currentBackward にも適用する。
GameEngine::Vector3 RearCameraGravityUp::SmoothGravityUp(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const RearCameraAimState& aim,
   RearCameraDirectionTracker& direction,
   float deltaTime) {
   using namespace GameEngine;

   // 着地候補惑星へ接近している間は、接触前から予測着地法線へ基準Upを移す。
   // 惑星切替の確定を待つと接触フレームで目標Upが変わり、急なロールの原因になる。
   Vector3 targetUp = input.gravityUp;
   if (input.isAirborne && transition.currentPreLandingBlend > 1e-4f) {
	  targetUp = BlendUnitDirectionSafely(
		 targetUp,
		 input.predictedLandingUp,
		 transition.currentPreLandingBlend,
		 targetUp,
		 aim.cachedRight);
   }

   // 目標 up を正規化
   float targetLen = targetUp.Length();
   if (targetLen < 1e-6f) targetUp = { 0.0f, 1.0f, 0.0f };
   else targetUp = targetUp * (1.0f / targetLen);

   // 現在 up も正規化（数値誤差が蓄積しないよう毎フレーム実施）
   float curLen = state_.currentGravityUp.Length();
   if (curLen < 1e-6f) state_.currentGravityUp = targetUp;
   else state_.currentGravityUp = state_.currentGravityUp * (1.0f / curLen);

   // 回転前の Up を保存しておく（後でデルタ回転を算出するため）
   Vector3 oldUp = state_.currentGravityUp;

   // 角速度制限付きで目標 Up へ追従（180°近傍でも破綻しない）
   float maxRadiansDelta = std::min(
	  settings.gravityUpLerpSpeed * std::max(0.0f, deltaTime),
	  GameEngine::MathConstants::kPi - 1e-4f);
   state_.currentGravityUp = RotateTowardsUnit(
	  oldUp,
	  targetUp,
	  maxRadiansDelta,
	  aim.cachedRight);

   if (direction.IsEnabled()) {
      direction.TransportWithGravity(oldUp, state_.currentGravityUp);
   }
   return state_.currentGravityUp;
}

void RearCameraGravityUp::Reset(const RearCameraSettings&) {
   state_ = {};
}

} // namespace App
