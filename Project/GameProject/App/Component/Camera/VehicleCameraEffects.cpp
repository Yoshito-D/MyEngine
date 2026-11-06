#include "GameProject/App/Component/Camera/VehicleCameraEffects.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"
#include "GameEngine/Scene/Camera/Components/PerlinNoise.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include <algorithm>
#include <cmath>

namespace App {

namespace {
GameEngine::PerlinNoise* GetOrAddShakeNoise(GameEngine::ICinemachineComponent& cameraComponent) {
   auto* virtualCamera = cameraComponent.GetOwnerCamera();

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
}

void VehicleCameraEffects::Reset(const VehicleEventSnapshot& snapshot) {
   eventCursor_.Reset(snapshot);
   currentDriftDistanceScale_ = 1.0f;
   currentDriftCameraHeightOffset_ = 0.0f;
   currentDriftLookTargetHeightOffset_ = 0.0f;
}

void VehicleCameraEffects::Update(const VehicleCameraEffectsSettings& settings, float deltaTime, bool steering,
   const VehicleEventSnapshot& snapshot, const GameEngine::Vector3& gravityUp, std::span<IVehicleCamera* const> cameras,
   const GameEngine::VirtualCamera* selectedCamera) {
   // ドリフト成立を待たず、デッドゾーン処理済みの横入力で接近を始める。
   // 空中のロール入力では接近させず、地上の横入力中だけ演出を有効にする。
   const bool active = settings.enableDriftCameraApproach && steering;
   const float distance = active ? 1.0f - settings.driftCameraDistanceReduction : 1.0f;
   const float height = active ? settings.driftCameraHeightOffset : 0.0f;
   const float lookHeight = active ? settings.driftLookTargetHeightOffset : 0.0f;
   const float blend = 1.0f - std::exp(-settings.driftCameraLerpSpeed * std::max(0.0f, deltaTime));
   currentDriftDistanceScale_ += (distance - currentDriftDistanceScale_) * blend;
   currentDriftCameraHeightOffset_ += (height - currentDriftCameraHeightOffset_) * blend;
   currentDriftLookTargetHeightOffset_ += (lookHeight - currentDriftLookTargetHeightOffset_) * blend;
   const VehicleCameraEffectInput effect{
      currentDriftDistanceScale_, currentDriftCameraHeightOffset_, currentDriftLookTargetHeightOffset_};

   // 無効中の通知も既読にし、再有効化した瞬間の遅延発火を防ぐ。
   const auto changes = eventCursor_.Read(snapshot);

   for (auto* camera : cameras) {
      if (!camera) {
         continue;
      }

      // 全候補の追従入力を新しく保ち、画面固有のシェイクだけを選択先へ限定する。
      camera->ApplyVehicleEffectInput(effect);
      auto& component = camera->GetVehicleCameraComponent();

      if (!component.IsEnabled() || (selectedCamera && component.GetOwnerCamera() != selectedCamera)) {
         continue;
      }

      if (settings.enableLandingShake && changes.landed) {
         if (auto* noise = GetOrAddShakeNoise(component)) {
            const auto direction = RearCameraMath::NormalizeOrFallback(camera->GetVehicleCameraUp(), gravityUp);
            noise->ShakeDirectional(direction, settings.landingShakeAmplitude, settings.landingShakeFrequency,
               settings.landingShakeDuration);
         }
      }

      if (settings.enableMiniTurboShake && changes.miniTurbo && settings.miniTurboShakeDuration > 0.0f &&
          (settings.miniTurboShakeHorizontalAmplitude > 0.0f || settings.miniTurboShakeVerticalAmplitude > 0.0f)) {
         // 同フレームに着地してもミニターボの縦横シェイクを優先する。
         if (auto* noise = GetOrAddShakeNoise(component)) {
            noise->ShakeScreenSpace(settings.miniTurboShakeHorizontalAmplitude,
               settings.miniTurboShakeVerticalAmplitude, settings.miniTurboShakeFrequency,
               settings.miniTurboShakeDuration);
         }
      }
   }
}

} // namespace App
