#include "GameProject/App/Component/Camera/RearFollow/RearCameraSpeedEffects.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraSpeedEffects::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera)
      return;
   const auto& transition = *GetOwnerCamera()->GetComponent<RearCameraTransition>();
   boostAlpha_ =
      UpdateAccelerationEffect(camera->GetInput(), camera->DescribeSettings(), transition.GetState(), state, deltaTime);
}

/// @brief 通常FOVと演出による追加FOVを合成し state.fov へ反映する
/// @details 通常FOVの編集は直接反映し、速度・空中演出の追加量だけを補間する。
///          加えて、boostAlpha を返すことで距離ブースト（ComputeEye）と
///          同じ速度スケールを共有できるようにする。
/// @return boostAlpha [0, 1]（加速の強さ。距離ブーストにも流用）
float RearCameraSpeedEffects::UpdateAccelerationEffect(const RearCameraInput& input, const RearCameraSettings& settings,
   const RearCameraTransitionState& transition, GameEngine::CameraState& state, float deltaTime) {
   // 速度が [settings.speedBoostThreshold, settings.speedBoostMax] の範囲にどれだけ入っているかを正規化する
   float speedRange = settings.speedBoostMax - settings.speedBoostThreshold;
   float boostAlpha = 0.0f;
   if (speedRange > 1e-4f) {
      boostAlpha = std::clamp((input.playerSpeed - settings.speedBoostThreshold) / speedRange, 0.0f, 1.0f);
   }

   // ─────────────────────────────────────────────────────────────
   // インパルス方式Spring による加速キック
   //
   // 【旧設計の問題点】
   //   目標値 = speedDelta として Spring に追従させると、翌フレームでは
   //   speedDelta が 0 に戻るため Spring が即座に逆向きに引き戻され、
   //   ミニターボ解放直後の演出がほぼ 1 フレームで消えてしまっていた。
   //
   // 【インパルス方式の考え方】
   //   Spring の目標は「常に 0（自然長）」に固定する。
   //   加速が検出されたフレームだけ Spring の速度（velocity）へ直接
   //   インパルス（瞬間的な蹴り）を与える。
   //   以降は Spring の復元力と減衰だけで自然に 0 へ収束するため、
   //   ミニターボ特有の「瞬間的に広がって徐々に戻る」演出が得られる。
   // ─────────────────────────────────────────────────────────────
   float speedDelta = 0.0f;
   if (!state_.isSpeedInitialized) {
      state_.previousPlayerSpeed = input.playerSpeed;
      state_.isSpeedInitialized = true;
   } else {
      speedDelta = input.playerSpeed - state_.previousPlayerSpeed;
      state_.previousPlayerSpeed = input.playerSpeed;
   }

   if (speedDelta > 0.0f) {
      // 同じ速度差へ係数とターボ量を二重加算せず、最大キック量だけで強さを決める。
      float kickAlpha = std::clamp(speedDelta / kSpeedDeltaForMaxKick, 0.0f, 1.0f);
      float fovImpulse = kickAlpha * std::max(0.0f, settings.speedChangeFovKickMax);
      float distImpulse = kickAlpha * std::max(0.0f, settings.speedChangeDistanceKickMax);

      state_.springFovVelocity += fovImpulse * settings.springStiffness;
      state_.springDistanceVelocity += distImpulse * settings.springStiffness;
   } else if (speedDelta < 0.0f && input.playerSpeed < input.autoSpeed) {
      // input.autoSpeed 以下に落ちたときのみ逆向きキックを与え、FOV を絞りつつカメラを近づける
      // ブースト後の autoSpeed への自然回復中は発火しない
      float decel = -speedDelta;
      float kickAlpha = std::clamp(decel / kSpeedDeltaForMaxKick, 0.0f, 1.0f);
      float fovImpulse = kickAlpha * std::max(0.0f, settings.speedChangeFovKickMax);
      float distImpulse = kickAlpha * std::max(0.0f, settings.speedChangeDistanceKickMax);

      state_.springFovVelocity -= fovImpulse * settings.springStiffness;
      state_.springDistanceVelocity -= distImpulse * settings.springStiffness;
   }

   // 目標は常に 0（自然長）。Spring の復元力と減衰で収束させる。
   StepSpring1D(0.0f, settings.springStiffness, settings.springDamping, deltaTime, state_.springFovOffset,
      state_.springFovVelocity);
   StepSpring1D(0.0f, settings.springStiffness, settings.springDamping, deltaTime, state_.springDistanceOffset,
      state_.springDistanceVelocity);

   // 通常FOVは直接使い、速度比例ブースト・空中ブースト・Springキックだけを補間する。
   float baseFov = ClampCameraFov(settings.fovDefault);
   float targetFovOffset = std::max(0.0f, settings.fovBoostMax) * boostAlpha +
                           std::max(0.0f, settings.airborneFovOffset) * transition.currentAirborneBlend +
                           state_.springFovOffset;
   targetFovOffset = ClampCameraFov(baseFov + targetFovOffset) - baseFov;

   // 基準値の編集を遅い演出補間に混ぜず、演出の履歴を保ったまま次の更新で画角へ反映する。
   if (!std::isfinite(state_.currentFovOffset)) {
      state_.currentFovOffset = 0.0f;
   }
   float t = ExpSmoothingFactor(settings.fovLerpSpeed, deltaTime);
   state_.currentFovOffset += (targetFovOffset - state_.currentFovOffset) * t;
   state_.currentFov = ClampCameraFov(baseFov + state_.currentFovOffset);
   state.fov = state_.currentFov;

   return boostAlpha;
}

void RearCameraSpeedEffects::Reset(const RearCameraSettings& settings) {
   boostAlpha_ = 0.0f;
   state_.currentFov = ClampCameraFov(settings.fovDefault);
   state_.currentFovOffset = 0.0f;
   state_.springFovOffset = 0.0f;
   state_.springFovVelocity = 0.0f;
   state_.springDistanceOffset = 0.0f;
   state_.springDistanceVelocity = 0.0f;
   state_.previousPlayerSpeed = 0.0f;
   state_.isSpeedInitialized = false;
}

} // namespace App
