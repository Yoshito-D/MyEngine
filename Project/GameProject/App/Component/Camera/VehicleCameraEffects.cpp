#include "GameProject/App/Component/Camera/VehicleCameraEffects.h"
#include "GameProject/App/Component/Camera/GravityFollowCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameProject/App/Component/Camera/PlanetLeashCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"
#include "GameEngine/Scene/Camera/Components/PerlinNoise.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"

namespace App {
void VehicleCameraEffects::Reset(uint64_t fireCount) {
   *this = {};
   lastMiniTurboFireCount_ = fireCount;
}

using RearCameraMath::NormalizeOrFallback;

namespace {
GameEngine::PerlinNoise* GetOrAddShakeNoise(GameEngine::ICinemachineComponent* cameraComponent) {
   if (!cameraComponent) {
      return nullptr;
   }

   auto* virtualCamera = cameraComponent->GetOwnerCamera();

   if (!virtualCamera) {
      return nullptr;
   }

   auto* noise = virtualCamera->GetComponent<GameEngine::PerlinNoise>();

   if (!noise) {
      // シェイク未設定のカメラでも演出を受けられるよう、必要時だけ追加する。
      noise = virtualCamera->AddComponent<GameEngine::PerlinNoise>();
   }

   return noise;
}

void TriggerDirectionalShake(GameEngine::ICinemachineComponent* cameraComponent, const GameEngine::Vector3& direction,
   float amplitude, float frequency, float duration) {
   if (auto* noise = GetOrAddShakeNoise(cameraComponent)) {
      noise->ShakeDirectional(direction, amplitude, frequency, duration);
   }
}
}

void VehicleCameraEffects::Update(const VehicleCameraEffectsSettings& settings, float deltaTime, bool steering,
   bool isGrounded, uint64_t miniTurboFireCount, const GameEngine::Vector3& gravityUp,
   GravityFollowCamera* gravityFollowCamera_, PlayerRearFollowCamera* playerRearFollowCamera_,
   PlanetLeashCamera* planetLeashCamera_, const GameEngine::VirtualCamera* selectedCamera) {
   // ドリフト成立を待たず、デッドゾーン処理済みの横入力で接近を始める。
   // 空中のロール入力では接近させず、地上の横入力中だけ演出を有効にする。
   const bool isDriftCameraActive = settings.enableDriftCameraApproach && steering;
   const float targetDriftDistanceScale = isDriftCameraActive ? 1.0f - settings.driftCameraDistanceReduction : 1.0f;
   const float targetDriftCameraHeightOffset = isDriftCameraActive ? settings.driftCameraHeightOffset : 0.0f;
   const float targetDriftLookTargetHeightOffset = isDriftCameraActive ? settings.driftLookTargetHeightOffset : 0.0f;
   const float driftBlend = 1.0f - std::exp(-settings.driftCameraLerpSpeed * std::max(0.0f, deltaTime));
   currentDriftDistanceScale_ += (targetDriftDistanceScale - currentDriftDistanceScale_) * driftBlend;
   currentDriftCameraHeightOffset_ += (targetDriftCameraHeightOffset - currentDriftCameraHeightOffset_) * driftBlend;
   currentDriftLookTargetHeightOffset_ +=
      (targetDriftLookTargetHeightOffset - currentDriftLookTargetHeightOffset_) * driftBlend;

   // 保存用の基準設定を上書きせず、どのカメラへ切り替えても同じドリフト演出を入力する。
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetDriftDistanceScale(currentDriftDistanceScale_);
      gravityFollowCamera_->SetDriftHeightOffsets(currentDriftCameraHeightOffset_, currentDriftLookTargetHeightOffset_);
   }

   if (playerRearFollowCamera_) {
      playerRearFollowCamera_->SetDriftDistanceScale(currentDriftDistanceScale_);
      playerRearFollowCamera_->SetDriftHeightOffsets(
         currentDriftCameraHeightOffset_, currentDriftLookTargetHeightOffset_);
   }

   if (planetLeashCamera_) {
      planetLeashCamera_->SetDriftDistanceScale(currentDriftDistanceScale_);
      planetLeashCamera_->SetDriftHeightOffsets(currentDriftCameraHeightOffset_, currentDriftLookTargetHeightOffset_);
   }

   // 全候補の追従入力を新しく保ち、画面固有のシェイク・撮影だけを選択先へ限定する。
   const auto IsSelectedCamera = [selectedCamera](const GameEngine::ICinemachineComponent* component) {
      return component && component->IsEnabled() && (!selectedCamera || component->GetOwnerCamera() == selectedCamera);
   };
   if (settings.enableLandingShake && isGrounded && !wasGrounded_) {
      // 接地中の連続発火を避け、各カメラ固有の画面Upに沿って同じ衝撃を与える。
      if (IsSelectedCamera(gravityFollowCamera_)) {
         TriggerDirectionalShake(gravityFollowCamera_,
            NormalizeOrFallback(gravityFollowCamera_->GetCameraUp(), gravityUp), settings.landingShakeAmplitude,
            settings.landingShakeFrequency, settings.landingShakeDuration);
      }

      if (IsSelectedCamera(playerRearFollowCamera_)) {
         TriggerDirectionalShake(playerRearFollowCamera_,
            NormalizeOrFallback(playerRearFollowCamera_->GetCameraUp(), gravityUp), settings.landingShakeAmplitude,
            settings.landingShakeFrequency, settings.landingShakeDuration);
      }

      if (IsSelectedCamera(planetLeashCamera_)) {
         TriggerDirectionalShake(planetLeashCamera_, NormalizeOrFallback(planetLeashCamera_->GetCameraUp(), gravityUp),
            settings.landingShakeAmplitude, settings.landingShakeFrequency, settings.landingShakeDuration);
      }
   }

   wasGrounded_ = isGrounded;
   const bool miniTurboFired = miniTurboFireCount != lastMiniTurboFireCount_;

   // 無効中の通知も記録し、設定を再び有効にした瞬間の遅延発火を防ぐ。
   lastMiniTurboFireCount_ = miniTurboFireCount;

   if (settings.enableMiniTurboShake && miniTurboFired && settings.miniTurboShakeDuration > 0.0f &&
       (settings.miniTurboShakeHorizontalAmplitude > 0.0f || settings.miniTurboShakeVerticalAmplitude > 0.0f)) {
      // 同フレームに着地してもミニターボの縦横シェイクを優先する。
      for (GameEngine::ICinemachineComponent* cameraComponent :
         {static_cast<GameEngine::ICinemachineComponent*>(gravityFollowCamera_),
            static_cast<GameEngine::ICinemachineComponent*>(playerRearFollowCamera_),
            static_cast<GameEngine::ICinemachineComponent*>(planetLeashCamera_)}) {
         if (IsSelectedCamera(cameraComponent)) {
            if (auto* noise = GetOrAddShakeNoise(cameraComponent)) {
               noise->ShakeScreenSpace(settings.miniTurboShakeHorizontalAmplitude,
                  settings.miniTurboShakeVerticalAmplitude, settings.miniTurboShakeFrequency,
                  settings.miniTurboShakeDuration);
            }
         }
      }
   }
}
}
