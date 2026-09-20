#include "RearCameraTransition.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraTransition::MutateCameraState(GameEngine::CameraState&, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& input = camera->GetInput();
   const auto& aim = *owner_->GetComponent<RearCameraAimSolver>();
   events_ = BeginFrame(input, aim.GetState().lastLookTargetOffset);
   UpdatePreLandingBlend(input, *camera, deltaTime);
   // この2つのブレンドはUp計算から独立しているため、同じ部品で先に確定できる。
   UpdateAirborneBlend(input, *camera, deltaTime);
   UpdatePlayerFramingBlend(input, *camera, deltaTime);
   EndFrame(input.isAirborne);
}

/// @brief 地上/空中ブレンド値を更新する
/// @details ジャンプ開始・着地で注視点、距離、FOV が一気に変わると画が跳ねるため、
///          共有ブレンド値を先に滑らかに動かして各パラメータへ適用する。
void RearCameraTransition::UpdateAirborneBlend(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   float deltaTime) {
   float targetBlend = input.isAirborne ? 1.0f : 0.0f;
   float t = ExpSmoothingFactor(settings.airborneBlendLerpSpeed, deltaTime);
   state_.currentAirborneBlend = std::clamp(
      state_.currentAirborneBlend + (targetBlend - state_.currentAirborneBlend) * t,
      0.0f,
      1.0f);
}

/// @brief 離陸/着地に応じてプレイヤーの画面位置ブレンドを更新する
/// @details 地上下側(0)と空中中央(1)の構図だけを独立して補間し、
///          距離・FOV・惑星ガイドに使う currentAirborneBlend へは影響させない。
void RearCameraTransition::UpdatePlayerFramingBlend(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   float deltaTime) {
   float targetBlend = input.isAirborne ? 1.0f : 0.0f;
   if (std::abs(targetBlend - state_.playerFramingBlendTarget) > 1e-4f) {
      state_.playerFramingBlendStart = state_.currentPlayerFramingBlend;
      state_.playerFramingBlendTarget = targetBlend;
      state_.playerFramingBlendElapsed = 0.0f;

      float configuredSeconds = input.isAirborne
         ? std::max(0.0f, settings.takeoffFramingBlendSeconds)
         : std::max(0.0f, settings.landingFramingBlendSeconds);
      // 途中で状態が反転した場合は残り距離に比例して時間を短縮し、
      // 0→1 / 1→0 の全区間が設定秒数になる速度感を維持する。
      state_.playerFramingBlendDuration = configuredSeconds
         * std::abs(state_.playerFramingBlendTarget - state_.playerFramingBlendStart);
   }

   if (state_.playerFramingBlendDuration <= 1e-4f) {
      state_.currentPlayerFramingBlend = state_.playerFramingBlendTarget;
      state_.playerFramingBlendElapsed = state_.playerFramingBlendDuration;
      return;
   }

   state_.playerFramingBlendElapsed = std::min(
      state_.playerFramingBlendElapsed + std::max(0.0f, deltaTime),
      state_.playerFramingBlendDuration);
   float progress = std::clamp(
      state_.playerFramingBlendElapsed / state_.playerFramingBlendDuration,
      0.0f,
      1.0f);
   float easedProgress = SmoothStep01(progress);
   state_.currentPlayerFramingBlend = std::clamp(
      state_.playerFramingBlendStart
         + (state_.playerFramingBlendTarget - state_.playerFramingBlendStart) * easedProgress,
      0.0f,
      1.0f);
}

/// @brief 予測接触までの時間から着地前補間量を更新する
/// @details 残り時間をSmoothstepへ変換した後、別の指数平滑で追従する。
///          予測が有効になった瞬間の補間量を直接適用せず、切り替わりの境界を画面へ出さない。
void RearCameraTransition::UpdatePreLandingBlend(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   float deltaTime) {
   float targetBlend = 0.0f;
   if (settings.enablePreLandingCamera && input.isAirborne && input.landingPredictionValid) {
	  float startSeconds = std::max(0.0f, settings.preLandingPredictionSeconds);
	  float fullSeconds = std::clamp(
		 settings.preLandingFullBlendSeconds,
		 0.0f,
		 std::max(0.0f, startSeconds - 0.001f));
	  float blendDuration = startSeconds - fullSeconds;
	  if (blendDuration <= 1e-4f) {
		 targetBlend = input.predictedLandingSeconds <= fullSeconds ? 1.0f : 0.0f;
	  } else {
		 float progress = (startSeconds - input.predictedLandingSeconds) / blendDuration;
		 targetBlend = SmoothStep01(progress);
	  }
   }

   float followSpeed = targetBlend >= state_.currentPreLandingBlend
	  ? settings.preLandingBlendLerpSpeed
	  : settings.preLandingReleaseLerpSpeed;
   float blendT = ExpSmoothingFactor(followSpeed, deltaTime);
   state_.currentPreLandingBlend = std::clamp(
	  state_.currentPreLandingBlend + (targetBlend - state_.currentPreLandingBlend) * blendT,
	  0.0f,
	  1.0f);
   if (state_.currentPreLandingBlend < 1e-4f) {
	  state_.currentPreLandingBlend = 0.0f;
	  if (!input.isAirborne) {
		 state_.isLandingReleaseActive = false;
		 state_.landingReleaseBlendStart = 0.0f;
	  }
   }
}

/// @brief 接地時の表示状態を地上復帰用スナップショットへ保存する
/// @details 予測接触点などのワールド座標を接地後も使うと、移動するプレイヤーから古い地点へ
///          カメラが引かれ続ける。ステートレスに再計算される注視点だけを相対オフセットで保存し、
///          プレイヤーと一緒に移動する状態として地上カメラへ戻す。
void RearCameraTransition::BeginLandingRelease(const GameEngine::Vector3& lastLookTargetOffset) {
   if (state_.currentPreLandingBlend <= 1e-4f) {
	  state_.isLandingReleaseActive = false;
	  state_.landingReleaseBlendStart = 0.0f;
	  return;
   }

   state_.isLandingReleaseActive = true;
   state_.landingReleaseBlendStart = state_.currentPreLandingBlend;
   state_.landingReleaseLookOffset = lastLookTargetOffset;
}

/// @brief 空中予測または接地時スナップショットを適用する現在の割合を返す
float RearCameraTransition::ComputePreLandingGuideBlend(const RearCameraInput& input) const {
   if (!input.isAirborne && state_.isLandingReleaseActive) {
	  float startBlend = std::max(state_.landingReleaseBlendStart, 1e-4f);
	  return std::clamp(state_.currentPreLandingBlend / startBlend, 0.0f, 1.0f);
   }
   return std::clamp(state_.currentPreLandingBlend, 0.0f, 1.0f);
}

RearCameraFrameEvents RearCameraTransition::BeginFrame(
   const RearCameraInput& input,
   const GameEngine::Vector3& lastLookTargetOffset) {
   bool justLanded = state_.wasAirborneLastFrame && !input.isAirborne;
   bool justTookOff = !state_.wasAirborneLastFrame && input.isAirborne;

   // 接地後に古い予測値を使い続けず、直前に画面へ出していた相対状態から地上へ戻す。
   if (justLanded) {
	  BeginLandingRelease(lastLookTargetOffset);
   } else if (justTookOff) {
	  // 前回着地の解除途中に再ジャンプしても、古いスナップショットを新しい予測へ混ぜない。
	  state_.isLandingReleaseActive = false;
	  state_.landingReleaseBlendStart = 0.0f;
	  state_.currentPreLandingBlend = 0.0f;
   }

   return { justLanded, justTookOff };
}

void RearCameraTransition::Reset(const RearCameraSettings& settings) {
   events_ = {};
   state_.wasAirborneLastFrame = false;
   state_.currentAirborneBlend = 0.0f;
   state_.currentPlayerFramingBlend = 0.0f;
   state_.playerFramingBlendStart = 0.0f;
   state_.playerFramingBlendTarget = 0.0f;
   state_.playerFramingBlendElapsed = 0.0f;
   state_.playerFramingBlendDuration = 0.0f;
   state_.currentPreLandingBlend = 0.0f;
   state_.isLandingReleaseActive = false;
   state_.landingReleaseBlendStart = 0.0f;
   state_.landingReleaseLookOffset = { 0.0f, settings.groundedTargetHeight, 0.0f };
}

} // namespace App
