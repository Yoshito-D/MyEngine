#include "RearCameraPlanetGuide.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraPlanetGuide::MutateCameraState(GameEngine::CameraState&, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& transition = *owner_->GetComponent<RearCameraTransition>();
   const auto& direction = *owner_->GetComponent<RearCameraDirectionTracker>();
   AdvanceTakeoffTimer(camera->GetInput(), *camera, transition.GetEvents(), direction.GetState().currentBackward, deltaTime);
   UpdatePlanetDirectionGuide(camera->GetInput(), *camera, direction.GetState().currentBackward, deltaTime);
}

/// @brief 空中時にカメラ方向を近傍惑星側へ寄せるための方向と係数を更新する
/// @details 注視点はプレイヤー中心のまま保ち、eye 側の回り込み方向だけを補間する。
///          補間開始前は現在の後方へ張り付け、重力条件を満たした瞬間の逆振れを防ぐ。
void RearCameraPlanetGuide::UpdatePlanetDirectionGuide(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const GameEngine::Vector3& currentBackward,
   float deltaTime) {
   GameEngine::Vector3 desiredBackward = currentBackward;
   if (input.isAirborne) {
	  GameEngine::Vector3 toPlanet = input.planetCenter - input.pivotTarget;
	  float toPlanetLength = toPlanet.Length();
	  if (toPlanetLength > 1e-4f) {
		 desiredBackward = toPlanet * (-1.0f / toPlanetLength);
	  }
	}

	float targetFactor = ComputePlanetDirectionGravityFactorTarget(input, settings);
	bool wasInactive = state_.currentPlanetDirectionGravityFactor <= 1e-4f;
	float currentFollowSpeed = ComputePlanetDirectionFollowSpeed(input, settings);
	float factorT = ExpSmoothingFactor(currentFollowSpeed, deltaTime);
	state_.currentPlanetDirectionGravityFactor = std::clamp(
	  state_.currentPlanetDirectionGravityFactor + (targetFactor - state_.currentPlanetDirectionGravityFactor) * factorT,
	  0.0f,
	  1.0f);

   bool wantsGuide = targetFactor > 1e-4f || state_.currentPlanetDirectionGravityFactor > 1e-4f;
   if (!state_.isPlanetBackwardInitialized || !wantsGuide) {
	  state_.currentPlanetBackward = currentBackward;
	  state_.isPlanetBackwardInitialized = true;
	  return;
   }

   if (wasInactive) {
	  state_.currentPlanetBackward = currentBackward;
   }

	float maxRadiansDelta = currentFollowSpeed * (std::max)(0.0f, deltaTime);
	state_.currentPlanetBackward = RotateTowardsUnit(state_.currentPlanetBackward, desiredBackward, maxRadiansDelta);
}

/// @brief 離陸後の待機と復帰を反映した惑星ガイド追従速度を計算する
/// @details 待機中は0を返す。待機終了後はSmoothstepで0から設定値へ戻し、
///          地上時は次回離陸へ影響しないよう常に設定値を返す。
float RearCameraPlanetGuide::ComputePlanetDirectionFollowSpeed(
   const RearCameraInput& input,
   const RearCameraSettings& settings) const {
	float configuredSpeed = std::max(0.0f, settings.airbornePlanetDirectionLerpSpeed);
	if (!input.isAirborne) {
	  return configuredSpeed;
	}

	float delaySeconds = std::max(0.0f, settings.jumpPlanetDirectionDelaySeconds);
	if (state_.jumpPlanetDirectionSpeedElapsed < delaySeconds) {
	  return 0.0f;
	}

	float restoreSeconds = std::max(0.0f, settings.jumpPlanetDirectionRestoreSeconds);
	if (restoreSeconds <= 1e-4f) {
	  return configuredSpeed;
	}

	float restoreProgress = std::clamp(
	  (state_.jumpPlanetDirectionSpeedElapsed - delaySeconds) / restoreSeconds,
	  0.0f,
	  1.0f);
	return configuredSpeed * SmoothStep01(restoreProgress);
}

/// @brief 速度方向と重力Down方向の近さから惑星方向補間の目標係数を計算する
/// @details 開始閾値と最大閾値の間を smoothstep でならし、bias で効き方を調整する。
///          開始閾値未満では惑星方向補間を始めず、最大閾値で最大係数になる。
float RearCameraPlanetGuide::ComputePlanetDirectionGravityFactorTarget(
   const RearCameraInput& input,
   const RearCameraSettings& settings) const {
   if (!settings.enableAirbornePlanetDirectionGuide || !input.isAirborne) {
	  return 0.0f;
   }

   if (!settings.enableAirborneGravityDirectionBoost) {
	  return 1.0f;
   }

   GameEngine::Vector3 toPlanet = input.planetCenter - input.pivotTarget;
   float toPlanetLength = toPlanet.Length();
   float velocityLength = input.playerVelocity.Length();
   if (toPlanetLength <= 1e-4f || velocityLength <= 1e-4f) {
	  return 0.0f;
   }

   GameEngine::Vector3 gravityDown = toPlanet * (1.0f / toPlanetLength);
   GameEngine::Vector3 velocityDir = input.playerVelocity * (1.0f / velocityLength);
   float gravityAlignment = std::clamp(velocityDir.Dot(gravityDown), 0.0f, 1.0f);

   float startThreshold = std::clamp(settings.airborneGravityDirectionBoostThreshold, 0.0f, 0.999f);
   float fullThreshold = std::clamp(settings.airborneGravityDirectionBoostFullThreshold, 0.0f, 1.0f);
   if (fullThreshold <= startThreshold + 0.001f) {
	  fullThreshold = std::min(1.0f, startThreshold + 0.001f);
   }
   float normalized = std::clamp(
	  (gravityAlignment - startThreshold) / (fullThreshold - startThreshold),
	  0.0f,
	  1.0f);

   // 閾値の境界で急に効き始めないよう、S字カーブへ変換してから bias をかける。
   float smoothed = normalized * normalized * (3.0f - 2.0f * normalized);
   float bias = std::max(0.01f, settings.airborneGravityDirectionBoostBias);
   return std::clamp(static_cast<float>(std::pow(smoothed, bias)), 0.0f, 1.0f);
}

/// @brief 現在の空中惑星方向補間量を返す
/// @details 基本補間量に対し、速度が惑星中心方向（重力Down方向）へ近いほど係数を上げる。
///          開始判定OFF時は係数目標を1にし、方向の立ち上がりだけは滑らかに保つ。
float RearCameraPlanetGuide::ComputePlanetDirectionBlend(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const GameEngine::Vector3& trackedBackward) const {
   if (!settings.enableAirbornePlanetDirectionGuide) {
	  return 0.0f;
   }

	float configuredMaxBlend = std::clamp(settings.airbornePlanetDirectionBlend, 0.0f, 1.0f);
	float cappedBlend = std::clamp(
	  transition.currentAirborneBlend * state_.currentPlanetDirectionGravityFactor * configuredMaxBlend,
	  0.0f,
	  configuredMaxBlend);
   if (cappedBlend <= 0.0f) {
	  return 0.0f;
   }

   GameEngine::Vector3 toPlanet = input.planetCenter - input.pivotTarget;
   float toPlanetLength = toPlanet.Length();
   if (toPlanetLength <= 1e-4f) {
	  return 0.0f;
   }

   GameEngine::Vector3 desiredPlanetBackward = toPlanet * (-1.0f / toPlanetLength);
   GameEngine::Vector3 currentBackward = NormalizeOrFallback(trackedBackward, desiredPlanetBackward);
   GameEngine::Vector3 guideBackward = NormalizeOrFallback(state_.currentPlanetBackward, desiredPlanetBackward);
   GameEngine::Vector3 cappedBackward = BlendUnitDirectionSafely(currentBackward, guideBackward, cappedBlend, currentBackward);
   float naturalPlanetVisibility = std::clamp(currentBackward.Dot(desiredPlanetBackward), -1.0f, 1.0f);
   float cappedPlanetVisibility = std::clamp(cappedBackward.Dot(desiredPlanetBackward), -1.0f, 1.0f);

   // 原因: 以前は惑星補間最大量と惑星方向への Dot 値を直接比較していたため、
   //       速度後方だけで既に惑星が映る場面でも最大量側へ戻す補正が発生して画が跳ねた。
   // 修正: 最大量を適用した候補方向を先に作り、自然な後方より惑星が見える場合だけ補正する。
   float visibilityGain = cappedPlanetVisibility - naturalPlanetVisibility;
   if (visibilityGain <= 1e-4f) {
	  return 0.0f;
   }

   // 改善量が 0 を跨いだ瞬間に補正をON/OFFすると小さく跳ねるため、
   // Dot の改善量も短くフェードさせ、最大量を超えない範囲で効き始めをならす。
   float usefulBlend = std::clamp(visibilityGain / kPlanetVisibilityGainFadeRange, 0.0f, 1.0f);
   usefulBlend = usefulBlend * usefulBlend * (3.0f - 2.0f * usefulBlend);
   return cappedBlend * usefulBlend;
}

void RearCameraPlanetGuide::AdvanceTakeoffTimer(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraFrameEvents& events,
   const GameEngine::Vector3& currentBackward,
   float deltaTime) {
	if (events.justTookOff) {
	  state_.jumpPlanetDirectionSpeedElapsed = 0.0f;
	  state_.currentPlanetDirectionGravityFactor = 0.0f;
	  state_.currentPlanetBackward = currentBackward;
	  state_.isPlanetBackwardInitialized = true;
	}
	float jumpPlanetDirectionControlSeconds = std::max(0.0f, settings.jumpPlanetDirectionDelaySeconds)
	  + std::max(0.0f, settings.jumpPlanetDirectionRestoreSeconds);
	if (input.isAirborne) {
	  // 離陸直後はガイド方向を動かさず、待機終了後に追従速度そのものを徐々に戻す。
	  state_.jumpPlanetDirectionSpeedElapsed = std::min(
		 state_.jumpPlanetDirectionSpeedElapsed + std::max(0.0f, deltaTime),
		 jumpPlanetDirectionControlSeconds);
	} else {
	  state_.jumpPlanetDirectionSpeedElapsed = jumpPlanetDirectionControlSeconds;
	}

}

void RearCameraPlanetGuide::Reset(const RearCameraSettings& settings) {
   state_.currentPlanetDirectionGravityFactor = 0.0f;
   state_.jumpPlanetDirectionSpeedElapsed = std::max(0.0f, settings.jumpPlanetDirectionDelaySeconds)
	  + std::max(0.0f, settings.jumpPlanetDirectionRestoreSeconds);
   state_.currentPlanetBackward = GameEngine::Vector3{ 0.0f, 0.0f, -1.0f };
   state_.isPlanetBackwardInitialized = false;
}

} // namespace App
