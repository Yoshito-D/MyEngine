#include "GameProject/App/Component/Tutorial/TutorialProgressComponent.h"
#include "GameProject/App/Component/Tutorial/TutorialStagePlacement.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Collision/TriggerVolumeComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDistanceTextComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void TutorialProgressComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   targetPlanet_ = sceneWorld.FindObjectById(targetPlanetObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);
   characterJump_ = playerObject_ ? playerObject_->GetComponent<CharacterJump>() : nullptr;
   characterLanding_ = playerObject_ ? playerObject_->GetComponent<CharacterLanding>() : nullptr;
   gravityBody_ = playerObject_ ? playerObject_->GetComponent<GravityBody>() : nullptr;
   planetSwitcher_ = playerObject_ ? playerObject_->GetComponent<PlanetSwitcher>() : nullptr;
   vehicleController_ = playerObject_ ? playerObject_->GetComponent<VehicleController>() : nullptr;
   vehicleInput_ = playerObject_ ? playerObject_->GetComponent<VehicleInputComponent>() : nullptr;
   vehicleDrift_ = playerObject_ ? playerObject_->GetComponent<VehicleDrift>() : nullptr;
   landingBoost_ = playerObject_ ? playerObject_->GetComponent<VehicleLandingBoost>() : nullptr;
   guideText_ = HasOwner() ? GetOwner().GetComponent<GameEngine::UITextComponent>() : nullptr;
   auto* feedback = sceneWorld.FindObjectById(feedbackObjectId_);
   feedbackText_ = feedback ? feedback->GetComponent<GameEngine::UITextComponent>() : nullptr;
   auto* prompt = sceneWorld.FindObjectById(promptObjectId_);
   promptText_ = prompt ? prompt->GetComponent<GameEngine::UITextComponent>() : nullptr;
   auto* distance = sceneWorld.FindObjectById(goalDistanceObjectId_);
   goalDistance_ = distance ? distance->GetComponent<RaceGoalDistanceTextComponent>() : nullptr;
   auto* direction = sceneWorld.FindObjectById(goalDirectionObjectId_);
   goalDirection_ = direction ? direction->GetComponent<RaceGoalDirectionHUDComponent>() : nullptr;
}

void TutorialProgressComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   controllerEnabledWhenPlaying_ = vehicleController_ && vehicleController_->IsEnabled();
   gravityEnabledWhenPlaying_ = gravityBody_ && gravityBody_->IsEnabled();
   miniTurboCount_ = 0;
   lastMiniTurboFireCount_ = vehicleDrift_ ? vehicleDrift_->GetMiniTurboFireCount() : 0;
   wasDrifting_ = false;
   wasAirborne_ = characterJump_ && characterJump_->IsJumping();
   jumpStarted_ = false;
   airControlStartUp_ = {};
   airControlInputReceived_ = false;
   airRotationPerformed_ = false;
   sceneChangeRequested_ = false;
   completionElapsed_ = 0.0f;
   runtimeInitialized_ = false;
   guidePresenter_.Reset(guideText_ ? guideText_->GetStyle().color.w : 1.0f);
   SetTargetPlanetEnabled(false);
   phase_ = Phase::DrivingIntro;
   if (vehicleInput_) {
      vehicleInput_->SetJumpEnabled(false);
   }

   SetGoalGuidanceEnabled(false);
   UpdatePresentation(0.0f);

   if (!HasRequiredReferences()) {
      Logger::Warning("Tutorial player, planet, goal, or HUD references are missing.", Logger::LogChannel::Game);
   }
}

void TutorialProgressComponent::UpdatePresentation(float deltaTime) {
   TutorialGuideState state;
   state.phase = phase_;
   state.gamepadConnected = IsGamepadConnected();
   state.miniTurboCount = miniTurboCount_;
   state.canFireMiniTurbo = vehicleDrift_ && vehicleDrift_->CanFireMiniTurbo();
   state.isDrifting = vehicleDrift_ && vehicleDrift_->IsDrifting();
   state.isAirborne = characterJump_ && characterJump_->IsJumping();
   state.airRotationPerformed = airRotationPerformed_;
   const auto* transform = playerObject_ ? playerObject_->GetComponent<GameEngine::TransformComponent>() : nullptr;
   if (transform && gravityBody_) {
      const auto up = GameEngine::RotateVector({0.0f, 1.0f, 0.0f}, transform->GetLocalPose().GetActiveQuaternion());
      state.aligned = up.Dot(gravityBody_->GetTargetUpVector()) >= 0.95f;
   }
   guidePresenter_.UpdateGuideText(state, guideText_, promptText_, deltaTime, guideFadeDuration_);
   guidePresenter_.UpdateFeedback(state, feedbackText_, deltaTime);
   guidePresenter_.UpdateLayout(guideText_, promptText_, feedbackText_);
}

bool TutorialProgressComponent::HasRequiredReferences() const {
   return guideText_ && feedbackText_ && promptText_ && playerObject_ && targetPlanet_ && goalObject_ && characterJump_ &&
          characterLanding_ && gravityBody_ && planetSwitcher_ && vehicleController_ && vehicleInput_ &&
          vehicleDrift_ && landingBoost_ && goalDistance_ && goalDirection_ &&
          playerObject_->GetComponent<GameEngine::TransformComponent>() &&
          targetPlanet_->GetComponent<GameEngine::TransformComponent>() &&
          targetPlanet_->GetComponent<SphericalGravityAttractor>() &&
          goalObject_->GetComponent<GameEngine::TransformComponent>() &&
          goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>();
}

bool TutorialProgressComponent::IsGamepadConnected() const {
   return vehicleInput_ && GameEngine::EngineContext::IsGamePadConnected(vehicleInput_->DescribeSettings().playerSlot);
}

bool TutorialProgressComponent::IsOnTargetPlanet() const {
   if (!planetSwitcher_ || !characterLanding_ || !characterJump_ || characterJump_->IsJumping() ||
       !characterLanding_->IsGrounded()) {
      return false;
   }

   GameEngine::Model* receiver = nullptr;
   GameEngine::Vector3 center{};
   float radius = 0.0f;
   return planetSwitcher_->TryGetLandingPlanet(receiver, center, radius) && receiver == targetPlanet_;
}

void TutorialProgressComponent::Update(float deltaTime) {
   if (!runtimeInitialized_) {
      // 編集用のシーンに実行時の有効状態の変更を混入させない。
      runtimeInitialized_ = true;
      SetPhase(Phase::DrivingIntro);
   }

   if (!HasRequiredReferences()) {
      SetPlayerPaused(true);
      SetGoalGuidanceEnabled(false);

      if (guideText_) {
         guideText_->SetText(
            "チュートリアルの参照設定が不足しています。プレイヤー・惑星・ゴール・UIを確認してください。");
      }

      return;
   }

   const bool isAirborne = characterJump_->IsJumping();
   const bool tookOff = isAirborne && !wasAirborne_;
   const bool landed = !isAirborne && wasAirborne_ && characterLanding_->IsGrounded();
   const bool confirm = vehicleInput_->IsConfirmTriggered() && guidePresenter_.IsReady();

   switch (phase_) {
      case Phase::DrivingIntro:
         if (confirm) {
            SetPhase(Phase::DriftIntro);
         }

         break;
      case Phase::DriftIntro:
         if (confirm) {
            lastMiniTurboFireCount_ = vehicleDrift_->GetMiniTurboFireCount();
            SetPhase(Phase::DriftPractice);
         }

         break;
      case Phase::DriftPractice: {
         const auto fireCount = vehicleDrift_->GetMiniTurboFireCount();

         if (fireCount > lastMiniTurboFireCount_) {
            const auto remaining = static_cast<std::uint64_t>(kRequiredMiniTurbos - miniTurboCount_);
            miniTurboCount_ += static_cast<std::int32_t>(std::min(fireCount - lastMiniTurboFireCount_, remaining));
            guidePresenter_.ShowDriftResult(true);
         } else if (wasDrifting_ && !vehicleDrift_->IsDrifting()) {
            guidePresenter_.ShowDriftResult(false);
         }

         lastMiniTurboFireCount_ = fireCount;

         if (miniTurboCount_ >= kRequiredMiniTurbos) {
            SetPhase(Phase::JumpIntro);
         }

         break;
      }
      case Phase::JumpIntro:
         if (confirm) {
            jumpStarted_ = false;
            SetPhase(Phase::JumpPractice);
         }

         break;
      case Phase::JumpPractice:
         jumpStarted_ = jumpStarted_ || tookOff;

         if (jumpStarted_ && landed) {
            guidePresenter_.ShowLandingResult(landingBoost_->GetLastLandingResult());
            SetPhase(Phase::AirControlIntro);
         }

         break;
      case Phase::AirControlIntro:
         if (confirm) {
            SetPhase(Phase::AirControlPractice);
         }

         break;
      case Phase::AirControlPractice: {
         const auto* transform = playerObject_->GetComponent<GameEngine::TransformComponent>();
         const auto up = GameEngine::RotateVector({0.0f, 1.0f, 0.0f}, transform->GetLocalPose().GetActiveQuaternion());

         if (tookOff) {
            airControlStartUp_ = up;
            airControlInputReceived_ = false;
            airRotationPerformed_ = false;
         }

         if (isAirborne) {
            airControlInputReceived_ = airControlInputReceived_ ||
               std::abs(vehicleInput_->GetPitchInput()) > 0.1f || std::abs(vehicleInput_->GetRollInput()) > 0.1f;
            // 自動的な姿勢変化や一瞬の入力だけでは達成せず、離陸時から30度以上の回転を体験させる。
            airRotationPerformed_ = airRotationPerformed_ ||
               (airControlInputReceived_ && up.Dot(airControlStartUp_) <= 0.8660254f);
         }

         if (landed) {
            guidePresenter_.ShowLandingResult(landingBoost_->GetLastLandingResult());

            if (airRotationPerformed_ && RevealTargetPlanet()) {
               SetPhase(Phase::GoalGuidanceIntro);
            }
         }

         break;
      }
      case Phase::GoalGuidanceIntro:
         if (confirm) {
            SetPhase(Phase::PlanetTransfer);
         }

         break;
      case Phase::PlanetTransfer:
      case Phase::Goal:
         if (landed) {
            guidePresenter_.ShowLandingResult(landingBoost_->GetLastLandingResult());

            if (!IsOnTargetPlanet()) {
               guidePresenter_.ShowReturnToSource();
               SetPhase(Phase::PlanetTransfer);
            }
         }

         if (IsOnTargetPlanet()) {
            if (phase_ != Phase::Goal) {
               SetPhase(Phase::Goal);
            }

            // 滞在状態で判定し、着地とトリガー侵入の更新順による取りこぼしを避ける。
            if (goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>()->IsInside()) {
               SetPhase(Phase::Complete);
            }
         }

         break;
      case Phase::Complete:
         completionElapsed_ += std::max(deltaTime, 0.0f);

         // クリア文を読む時間を確保し、遷移要求は一度だけ発行する。
         if (!sceneChangeRequested_ && !nextScene_.empty() && completionElapsed_ >= completionDelay_) {
            sceneChangeRequested_ = true;
            GameEngine::EngineContext::ChangeScene(nextScene_);
         }

         break;
   }

   wasAirborne_ = isAirborne;
   wasDrifting_ = vehicleDrift_->IsDrifting();
   UpdatePresentation(deltaTime);
}

void TutorialProgressComponent::SetPhase(Phase phase) {
   phase_ = phase;

   if (phase_ == Phase::Complete) {
      completionElapsed_ = 0.0f;
   }

   // 説明中も自動走行を続け、ゴール到達後のクリア表示で停止する。
   SetPlayerPaused(phase_ == Phase::Complete);

   if (vehicleInput_) {
      vehicleInput_->SetJumpEnabled(
         phase_ == Phase::JumpPractice || phase_ == Phase::AirControlPractice ||
         phase_ == Phase::PlanetTransfer || phase_ == Phase::Goal);
   }

   SetGoalGuidanceEnabled(
      phase_ == Phase::GoalGuidanceIntro || phase_ == Phase::PlanetTransfer || phase_ == Phase::Goal);
}

void TutorialProgressComponent::SetPlayerPaused(bool paused) {
   // クリア表示中や参照不足時は、位置を保ったまま走行制御と物理を止める。
   if (gravityBody_) {
      if (paused) {
         gravityBody_->StopMotion();
      }

      gravityBody_->SetEnabled(!paused && gravityEnabledWhenPlaying_);
   }

   if (vehicleController_) {
      vehicleController_->SetEnabled(!paused && controllerEnabledWhenPlaying_);
   }
}

void TutorialProgressComponent::SetTargetPlanetEnabled(bool enabled) {
   if (!targetPlanet_) return;

   if (auto* gravity = targetPlanet_->GetComponent<SphericalGravityAttractor>()) {
      gravity->SetEnabled(enabled);
   }

   if (auto* render = targetPlanet_->GetComponent<GameEngine::RenderComponent>()) {
      auto settings = render->DescribeSettings();
      settings.visible = enabled;
      render->Configure(settings);
   }
}

void TutorialProgressComponent::SetGoalGuidanceEnabled(bool enabled) {
   if (goalDistance_) {
      goalDistance_->SetGuidanceEnabled(enabled);
   }

   if (goalDirection_) {
      goalDirection_->SetGuidanceEnabled(enabled);
   }

   if (goalObject_) {
      if (auto* render = goalObject_->GetComponent<GameEngine::RenderComponent>()) {
         auto settings = render->DescribeSettings();
         // クリア時は誘導と到達判定だけを止め、ゴール本体はシーン遷移まで表示する。
         settings.visible = enabled || phase_ == Phase::Complete;
         render->Configure(settings);
      }

      if (auto* trigger = goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>()) {
         trigger->SetEnabled(enabled);
      }
   }
}

bool TutorialProgressComponent::RevealTargetPlanet() {
   auto* playerTransform = playerObject_->GetComponent<GameEngine::TransformComponent>();
   auto* targetTransform = targetPlanet_->GetComponent<GameEngine::TransformComponent>();
   auto* goalTransform = goalObject_->GetComponent<GameEngine::TransformComponent>();
   GameEngine::Vector3 sourceCenter{};
   float sourceRadius = 0.0f;

   if (!playerTransform || !targetTransform || !goalTransform ||
       !planetSwitcher_->TryGetLandingPlanet(sourceCenter, sourceRadius)) {
      return false;
   }

   auto targetPose = targetTransform->GetLocalPose();
   auto goalPose = goalTransform->GetLocalPose();
   if (!PlaceTutorialDestination(
          playerTransform->GetLocalPose(), sourceCenter, sourceRadius, transferSurfaceGap_, targetPose, goalPose))
      return false;
   targetTransform->ApplyLocalPose(targetPose);
   goalTransform->ApplyLocalPose(goalPose);
   guidePresenter_.ClearLandingFeedback();
   SetTargetPlanetEnabled(true);
   return true;
}

nlohmann::json TutorialProgressComponent::Serialize() const {
   return nlohmann::json{{"playerObjectId", playerObjectId_}, {"targetPlanetObjectId", targetPlanetObjectId_},
      {"goalObjectId", goalObjectId_}, {"feedbackObjectId", feedbackObjectId_}, {"promptObjectId", promptObjectId_},
      {"goalDistanceObjectId", goalDistanceObjectId_}, {"goalDirectionObjectId", goalDirectionObjectId_},
      {"nextScene", nextScene_}, {"completionDelay", completionDelay_}, {"guideFadeDuration", guideFadeDuration_},
      {"transferSurfaceGap", transferSurfaceGap_}};
}

nlohmann::json TutorialProgressComponent::GetSharedParameters() const {
   return ExtractSharedParameters({"playerObjectId", "targetPlanetObjectId", "goalObjectId", "feedbackObjectId",
      "promptObjectId", "goalDistanceObjectId", "goalDirectionObjectId", "nextScene"});
}

void TutorialProgressComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void TutorialProgressComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void TutorialProgressComponent::ReadParameters(const nlohmann::json& data) {
   if (!data.is_object()) return;

   auto readId = [&data](const char* key, std::string& value) {
      if (data.contains(key) && data.at(key).is_string()) {
         value = data.at(key).get<std::string>();
      }
   };
   readId("playerObjectId", playerObjectId_);
   readId("targetPlanetObjectId", targetPlanetObjectId_);
   readId("goalObjectId", goalObjectId_);
   readId("feedbackObjectId", feedbackObjectId_);
   readId("promptObjectId", promptObjectId_);
   readId("goalDistanceObjectId", goalDistanceObjectId_);
   readId("goalDirectionObjectId", goalDirectionObjectId_);
   readId("nextScene", nextScene_);

   if (data.contains("completionDelay") && data.at("completionDelay").is_number()) {
      const float delay = data.at("completionDelay").get<float>();
      completionDelay_ = std::isfinite(delay) ? std::max(delay, 0.0f) : 2.5f;
   }

   if (data.contains("guideFadeDuration") && data.at("guideFadeDuration").is_number()) {
      const float duration = data.at("guideFadeDuration").get<float>();
      guideFadeDuration_ = std::isfinite(duration) ? std::max(duration, 0.0001f) : 0.2f;
   }

   if (data.contains("transferSurfaceGap") && data.at("transferSurfaceGap").is_number()) {
      const float gap = data.at("transferSurfaceGap").get<float>();
      transferSurfaceGap_ = std::isfinite(gap) ? std::max(gap, 10.0f) : 50.0f;
   }
}

const char* TutorialProgressComponent::GetPhaseName() const {
   switch (phase_) {
      case Phase::DrivingIntro:
         return "DrivingIntro";
      case Phase::DriftIntro:
         return "DriftIntro";
      case Phase::DriftPractice:
         return "DriftPractice";
      case Phase::JumpIntro:
         return "JumpIntro";
      case Phase::JumpPractice:
         return "JumpPractice";
      case Phase::AirControlIntro:
         return "AirControlIntro";
      case Phase::AirControlPractice:
         return "AirControlPractice";
      case Phase::GoalGuidanceIntro:
         return "GoalGuidanceIntro";
      case Phase::PlanetTransfer:
         return "PlanetTransfer";
      case Phase::Goal:
         return "Goal";
      case Phase::Complete:
         return "Complete";
   }

   return "Unknown";
}

#ifdef USE_IMGUI
void TutorialProgressComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   GameEngine::EditorUI::ObjectReference("Player", playerObjectId_, "TransformComponent");
   GameEngine::EditorUI::ObjectReference("Target Planet", targetPlanetObjectId_, "SphericalGravityAttractor");
   GameEngine::EditorUI::ObjectReference("Goal", goalObjectId_, "TriggerVolumeComponent");
   GameEngine::EditorUI::ObjectReference("Feedback Text", feedbackObjectId_, "UITextComponent");
   GameEngine::EditorUI::ObjectReference("Prompt Text", promptObjectId_, "UITextComponent");
   GameEngine::EditorUI::ObjectReference("Goal Distance", goalDistanceObjectId_, "RaceGoalDistanceTextComponent");
   GameEngine::EditorUI::ObjectReference("Goal Direction", goalDirectionObjectId_, "RaceGoalDirectionHUDComponent");
   GameEngine::EditorUI::SceneReference("Next Scene", nextScene_);
   ImGui::DragFloat("Completion Delay", &completionDelay_, 0.1f, 0.0f, 10.0f, "%.1f s");
   ImGui::DragFloat("Guide Fade Duration", &guideFadeDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Transfer Surface Gap", &transferSurfaceGap_, 0.5f, 10.0f, 150.0f);
   ImGui::Text("Phase: %s  Mini Turbo: %d / %d", GetPhaseName(), miniTurboCount_, kRequiredMiniTurbos);
   ImGui::Text("References: %s", HasRequiredReferences() ? "Ready" : "Missing");
}
#endif

} // namespace App
