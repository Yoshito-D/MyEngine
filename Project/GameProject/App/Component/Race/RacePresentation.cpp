#include "GameProject/App/Component/Race/RacePresentation.h"
#include <cstdint>

#include "GameProject/App/Component/Camera/CameraModeSwitcher.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Rendering/PlanetDistanceColorComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleSpeedPostEffectController.h"
#include "GameEngine/Utility/Logger.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/SceneWorld.h"

namespace App {
void RacePresentation::Resolve(GameEngine::SceneWorld& sceneWorld, const std::string& playerObjectId) {
   world_ = &sceneWorld;
   playerObjectId_ = playerObjectId;
   RefreshPlayerReferences();

   if (!vehicleController_ && !playerObjectId.empty() && !sceneWorld.FindObjectById(playerObjectId)) {
      Logger::Warning("Race player object was not found: " + playerObjectId, Logger::LogChannel::Game);
   }
}

void RacePresentation::RefreshPlayerReferences() {
   // 前シーンの参照を残さず、設定されたPlayerを起点に関連コンポーネントをまとめて解決する。
   vehicleController_ = nullptr;
   vehicleInput_ = nullptr;
   gravityBody_ = nullptr;
   cameraSwitcher_ = nullptr;
   speedPostEffectController_ = nullptr;

   if (auto* playerObject = world_ ? world_->FindObjectById(playerObjectId_) : nullptr) {
      vehicleController_ = playerObject->GetComponent<VehicleController>();
      vehicleInput_ = playerObject->GetComponent<VehicleInputComponent>();
      gravityBody_ = playerObject->GetComponent<GravityBody>();
      cameraSwitcher_ = playerObject->GetComponent<CameraModeSwitcher>();
      speedPostEffectController_ = playerObject->GetComponent<VehicleSpeedPostEffectController>();
   }
}

void RacePresentation::CaptureInitialState() {

   // エディタで指定した有効状態を保存し、レース中の一時ロック解除で上書きしない。
   vehicleControllerEnabledWhenUnlocked_ = vehicleController_ && vehicleController_->IsEnabled();
   vehicleInputEnabledWhenUnlocked_ = vehicleInput_ && vehicleInput_->IsEnabled();
   gravityBodyEnabledWhenUnlocked_ = gravityBody_ && gravityBody_->IsEnabled();
   cameraSwitcherEnabledWhenUnlocked_ = cameraSwitcher_ && cameraSwitcher_->IsEnabled();
   speedPostEffectEnabledWhenRacing_ = speedPostEffectController_ && speedPostEffectController_->IsEnabled();
}

void RacePresentation::ShowFinish(const std::string& finishCameraId) {
   RefreshPlayerReferences();

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

std::uint32_t RacePresentation::GetPlayerSlot() const {
   const auto* player = world_ ? world_->FindObjectById(playerObjectId_) : nullptr;
   const auto* input = player ? player->GetComponent<VehicleInputComponent>() : nullptr;
   return input ? input->DescribeSettings().playerSlot : VehicleInputComponent::Settings{}.playerSlot;
}

void RacePresentation::RestoreEffects() {
   RefreshPlayerReferences();

   if (cameraSwitcher_) {
      // 結果画面で停止した入力・演出制御を、シーン解決時に記録した有効状態へ戻す。
      cameraSwitcher_->SetEnabled(cameraSwitcherEnabledWhenUnlocked_);
   }

   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(speedPostEffectEnabledWhenRacing_);
   }
}

void RacePresentation::SetPlayerLocked(bool locked) {
   RefreshPlayerReferences();

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
   RefreshPlayerReferences();

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

void RacePresentation::BeginIntro(GameEngine::CinemachineBrain* brain, const StageIntroSettings& settings) {
   RefreshPlayerReferences();
   auto* introCamera = world_ ? world_->FindVirtualCamera(settings.cameraId) : nullptr;
   auto* countdownCamera = cameraSwitcher_ ? cameraSwitcher_->GetSelectedCamera() : nullptr;

   if (!brain || !introCamera || !countdownCamera || introCamera == countdownCamera || introCamera->IsActive() ||
       !introCamera->GetComponents().empty() || !settings.IsValid()) {
      Logger::Fail("Invalid stage intro camera or poses: " + settings.cameraId, Logger::LogChannel::Game);
   }

   for (const auto* camera : brain->GetVirtualCameras()) {
      if (camera->IsActive() && camera->GetPriority() >= introCamera->GetPriority()) {
         Logger::Fail("Stage intro camera must have higher priority: " + settings.cameraId, Logger::LogChannel::Game);
      }
   }

   if (!introSequence_.Begin(settings)) {
      Logger::Fail("Could not begin stage intro: " + settings.cameraId, Logger::LogChannel::Game);
   }

   introCameraId_ = settings.cameraId;
   countdownCameraId_ = countdownCamera->GetId();
   countdownCameraState_ = countdownCamera->GetState();
   introActive_ = true;
   PausePlanetColorUpdates();
   SetPlayerLocked(true);

   if (cameraSwitcher_) {
      cameraSwitcher_->SetEnabled(false);
   }

   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(false);
   }

   introCamera->SetState(*settings.goalFocusStartPose);
   introCamera->SetActive(true);
   brain->SetCameraMotionPaused(true);
   brain->Cut(introCamera);
}

StageIntroSequence::Frame RacePresentation::UpdateIntro(GameEngine::CinemachineBrain* brain, float deltaTime,
   bool inputTriggered, bool skipTriggered, bool sceneTransitioning) {
   auto* introCamera = world_ ? world_->FindVirtualCamera(introCameraId_) : nullptr;
   auto* countdownCamera = world_ ? world_->FindVirtualCamera(countdownCameraId_) : nullptr;

   if (!brain || !introCamera || !countdownCamera || !world_->FindObjectById(playerObjectId_)) {
      Logger::Fail("Stage intro lost a required reference: " + introCameraId_ + ", " + countdownCameraId_,
         Logger::LogChannel::Game);
   }

   const auto frame = introSequence_.Advance(deltaTime, inputTriggered, skipTriggered, sceneTransitioning);
   introCamera->SetState(frame.camera);

   if (frame.switchToCountdown) {
      RestoreIntroCamera(brain);

      // 完全な黒の間に距離補間を再開し、明転後に惑星のベース色が切り替わることを防ぐ。
      RestorePlanetColorUpdates();
   }

   return frame;
}

void RacePresentation::RestoreIntroCamera(GameEngine::CinemachineBrain* brain) {
   // Worldはカメラを先に破棄することがあるため、保存ポインターではなくIDで毎回生存確認する。
   if (auto* camera = world_ ? world_->FindVirtualCamera(introCameraId_) : nullptr) {
      camera->SetActive(false);
   }

   if (auto* camera = world_ ? world_->FindVirtualCamera(countdownCameraId_) : nullptr) {
      camera->SetState(countdownCameraState_);

      if (brain) {
         brain->Cut(camera);
      }
   }
}

void RacePresentation::PausePlanetColorUpdates() {
   pausedPlanetColorObjectIds_.clear();

   if (!world_) return;

   for (auto* object : world_->GetObjectStore().GetObjects()) {
      auto* color = object->GetComponent<PlanetDistanceColorComponent>();

      if (color && color->IsEnabled()) {
         // 無効化時の既存処理で設定済みのベース色へ戻し、カットシーン撮影中は距離によるグレー化を止める。
         pausedPlanetColorObjectIds_.push_back(object->GetEntityId());
         color->SetEnabled(false);
      }
   }
}

void RacePresentation::RestorePlanetColorUpdates() {
   for (const auto& objectId : pausedPlanetColorObjectIds_) {
      // 惑星が導入中やシーン終了処理で削除されても、破棄済みのコンポーネントへアクセスしない。
      auto* object = world_ ? world_->FindObjectById(objectId) : nullptr;
      auto* color = object ? object->GetComponent<PlanetDistanceColorComponent>() : nullptr;

      if (color) {
         color->SetEnabled(true);
      }
   }

   pausedPlanetColorObjectIds_.clear();
}

void RacePresentation::CancelIntro(GameEngine::CinemachineBrain* brain) {
   if (!introActive_) return;

   RestoreIntroCamera(brain);
   introSequence_.Reset();
   introActive_ = false;
   introCameraId_.clear();
   countdownCameraId_.clear();
   RestorePlanetColorUpdates();
   RestoreEffects();
   SetPlayerLocked(false);

   if (brain) {
      brain->SetCameraMotionPaused(false, 0.0f);
   }
}
}
