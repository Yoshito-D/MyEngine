#include "GameProject/App/Component/Race/RacePresentation.h"
#include <cstdint>

#include "GameProject/App/Component/Camera/CameraModeSwitcher.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleSpeedPostEffectController.h"
#include "GameEngine/Utility/Logger.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/SceneWorld.h"

namespace App {
void RacePresentation::Resolve(GameEngine::SceneWorld& sceneWorld, const std::string& playerObjectId) {
   // 前シーンの参照を残さず、設定されたPlayerを起点に関連コンポーネントをまとめて解決する。
   vehicleController_ = nullptr;
   vehicleInput_ = nullptr;
   gravityBody_ = nullptr;
   cameraSwitcher_ = nullptr;
   speedPostEffectController_ = nullptr;

   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId)) {
      vehicleController_ = playerObject->GetComponent<VehicleController>();
      vehicleInput_ = playerObject->GetComponent<VehicleInputComponent>();
      gravityBody_ = playerObject->GetComponent<GravityBody>();
      cameraSwitcher_ = playerObject->GetComponent<CameraModeSwitcher>();
      speedPostEffectController_ = playerObject->GetComponent<VehicleSpeedPostEffectController>();
   } else if (!playerObjectId.empty()) {
      Logger::Warning("Race player object was not found: " + playerObjectId, Logger::LogChannel::Game);
   }
}

void RacePresentation::CaptureInitialState() {
   playerLocked_ = false;

   // エディタで指定した有効状態を保存し、レース中の一時ロック解除で上書きしない。
   vehicleControllerEnabledWhenUnlocked_ = vehicleController_ && vehicleController_->IsEnabled();
   vehicleInputEnabledWhenUnlocked_ = vehicleInput_ && vehicleInput_->IsEnabled();
   gravityBodyEnabledWhenUnlocked_ = gravityBody_ && gravityBody_->IsEnabled();
   cameraSwitcherEnabledWhenUnlocked_ = cameraSwitcher_ && cameraSwitcher_->IsEnabled();
   speedPostEffectEnabledWhenRacing_ = speedPostEffectController_ && speedPostEffectController_->IsEnabled();
}

void RacePresentation::ShowFinish(const std::string& finishCameraId) {
   // 結果画面で停止後の速度値から加速演出が再有効化されないよう、制御元ごと停止する。
   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(false);
   }

   if (cameraSwitcher_ && !finishCameraId.empty()) {
      if (cameraSwitcher_->SwitchToCamera(finishCameraId)) {
         // 結果画面中に手動入力でゴールカメラから離れないよう固定する。
         cameraSwitcher_->SetEnabled(false);
      } else {
         Logger::Warning("Race finish camera was not found: " + finishCameraId, Logger::LogChannel::Game);
      }
   }
}

void RacePresentation::RestoreEffects() {
   if (cameraSwitcher_) {
      // 結果画面で停止した入力・演出制御を、シーン解決時に記録した有効状態へ戻す。
      cameraSwitcher_->SetEnabled(cameraSwitcherEnabledWhenUnlocked_);
   }

   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(speedPostEffectEnabledWhenRacing_);
   }
}

void RacePresentation::SetPlayerLocked(bool locked) {
   playerLocked_ = locked;

   // 走行とカメラ演出の入力を一緒に止め、リザルトの左右操作でドリフト接近を発火させない。
   // リザルトUIは別のUIアクションを直接読むため、選択と決定はそのまま受け付ける。
   if (vehicleInput_) {
      vehicleInput_->SetEnabled(!locked && vehicleInputEnabledWhenUnlocked_);
   }

   if (locked) {
      // 物理を止める前に残速度を消し、解除直後にロック前の慣性が再開しないようにする。
      if (gravityBody_) {
         gravityBody_->StopMotion();
         gravityBody_->SetEnabled(false);
      }

      if (vehicleController_) {
         vehicleController_->SetEnabled(false);
      }

      return;
   }

   // 解除時にも速度を中立化してから、シーン本来の有効状態だけを復元する。
   if (gravityBody_) {
      gravityBody_->StopMotion();
      gravityBody_->SetEnabled(gravityBodyEnabledWhenUnlocked_);
   }

   if (vehicleController_) {
      vehicleController_->SetEnabled(vehicleControllerEnabledWhenUnlocked_);
   }
}

void RacePresentation::SetCameraMotionPaused(GameEngine::CinemachineBrain* brain, bool paused, float blendSeconds,
   GameEngine::CinemachineBrain::BlendEasing easing) {
   if (brain) {
      brain->SetCameraMotionPaused(paused, blendSeconds, easing);
   }
}

void RacePresentation::ApplyCountdownCameraPose(GameEngine::CinemachineBrain* brain,
   const GameEngine::Vector3& position, const GameEngine::Vector3& rotation, float fov) {

   auto* camera = cameraSwitcher_ ? cameraSwitcher_->GetSelectedCamera() : (brain ? brain->GetActiveCamera() : nullptr);

   if (!camera) return;

   auto state = camera->GetState();
   state.transform.translation = position;
   state.transform.SetRotationEuler(rotation);
   state.hasViewMatrixOverride = false;
   state.fov = fov;
   camera->SetState(state);

   if (brain) {
      brain->Cut(camera);
   }
}
}
