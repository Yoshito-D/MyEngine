#include "TutorialProgressComponent.h"

#include "../Character/CharacterJump.h"
#include "../Character/CharacterLanding.h"
#include "../Gravity/PlanetSwitcher.h"
#include "../Vehicle/VehicleLandingBoost.h"
#include "Framework/EngineContext.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/BaseScene.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void TutorialProgressComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   characterJump_ = nullptr;
   characterLanding_ = nullptr;
   planetSwitcher_ = nullptr;
   landingBoost_ = nullptr;
   guideText_ = HasOwner()
      ? GetOwner().GetComponent<GameEngine::UITextComponent>()
      : nullptr;

   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      characterJump_ = playerObject->GetComponent<CharacterJump>();
      characterLanding_ = playerObject->GetComponent<CharacterLanding>();
      planetSwitcher_ = playerObject->GetComponent<PlanetSwitcher>();
      landingBoost_ = playerObject->GetComponent<VehicleLandingBoost>();
   }

   phase_ = Phase::Steering;
   completionElapsed_ = 0.0f;
   usedPitch_ = false;
   usedRoll_ = false;
   wasGrounded_ = characterLanding_ ? characterLanding_->IsGrounded() : true;
   sceneChangeRequested_ = false;
   landingFeedback_.clear();
   displayedText_.clear();
   UpdateGuideText();
}

void TutorialProgressComponent::Update(float deltaTime) {
   if (!guideText_ || !characterJump_ || !characterLanding_ || !planetSwitcher_ || !landingBoost_) {
      return;
   }

   const bool isGrounded = characterLanding_->IsGrounded();
   const bool isAirborne = characterJump_->IsJumping();
   const bool landedThisFrame = isGrounded && !wasGrounded_;

   const auto& steerAction =
      GameEngine::EngineContext::GetInputActionState("Gameplay", "Vehicle.Steer", 0);
   const auto& pitchAction =
      GameEngine::EngineContext::GetInputActionState("Gameplay", "Vehicle.Pitch", 0);
   const auto& rollAction =
      GameEngine::EngineContext::GetInputActionState("Gameplay", "Vehicle.Roll", 0);

   switch (phase_) {
      case Phase::Steering:
         if (std::abs(steerAction.value.x) >= 0.35f) {
            SetPhase(Phase::Jump);
         }
         break;
      case Phase::Jump:
         if (isAirborne) {
            SetPhase(Phase::AirControl);
         }
         break;
      case Phase::AirControl:
         if (isAirborne) {
            usedPitch_ = usedPitch_ || std::abs(pitchAction.value.x) >= 0.35f;
            usedRoll_ = usedRoll_ || std::abs(rollAction.value.x) >= 0.35f;
            if (usedPitch_ && usedRoll_) {
               SetPhase(Phase::PlanetTransfer);
            }
         }
         break;
      case Phase::PlanetTransfer:
      case Phase::LandingPractice:
         if (landedThisFrame) {
            HandleLandingResult();
         }
         break;
      case Phase::Complete:
         completionElapsed_ += std::max(deltaTime, 0.0f);
         if (!sceneChangeRequested_ && !nextScene_.empty() && completionElapsed_ >= completionDelay_) {
            sceneChangeRequested_ = true;
            GameEngine::BaseScene::SetNextSceneName(nextScene_);
         }
         break;
      default:
         break;
   }

   wasGrounded_ = isGrounded;
   UpdateGuideText();
}

nlohmann::json TutorialProgressComponent::Serialize() const {
   return nlohmann::json{
      { "playerObjectId", playerObjectId_ },
      { "targetPlanetIndex", targetPlanetIndex_ },
      { "nextScene", nextScene_ },
      { "completionDelay", completionDelay_ }
   };
}

void TutorialProgressComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
   if (data.contains("targetPlanetIndex") && data.at("targetPlanetIndex").is_number_integer()) {
      targetPlanetIndex_ = std::max(data.at("targetPlanetIndex").get<int>(), 0);
   }
   if (data.contains("nextScene") && data.at("nextScene").is_string()) {
      nextScene_ = data.at("nextScene").get<std::string>();
   }
   if (data.contains("completionDelay") && data.at("completionDelay").is_number()) {
      completionDelay_ = std::max(data.at("completionDelay").get<float>(), 0.0f);
   }
}

void TutorialProgressComponent::SetPhase(Phase phase) {
   if (phase_ == phase) {
      return;
   }
   phase_ = phase;
   if (phase_ == Phase::Complete) {
      completionElapsed_ = 0.0f;
   }
   displayedText_.clear();
}

void TutorialProgressComponent::UpdateGuideText() {
   if (!guideText_) {
      return;
   }
   const std::string guide = BuildGuideText();
   if (guide == displayedText_) {
      return;
   }
   guideText_->SetText(guide);
   displayedText_ = guide;
}

void TutorialProgressComponent::HandleLandingResult() {
   const int landedPlanetIndex = planetSwitcher_->GetCurrentPlanetIndex();
   if (phase_ == Phase::PlanetTransfer && landedPlanetIndex != targetPlanetIndex_) {
      landingFeedback_ =
         "元の惑星に戻りました。正面の青い惑星へ向けて、もう一度ジャンプしましょう。";
      return;
   }

   switch (landingBoost_->GetLastLandingResult()) {
      case LandingResult::Success:
         landingFeedback_.clear();
         SetPhase(Phase::Complete);
         break;
      case LandingResult::Normal:
         landingFeedback_ =
            "着地判定：NORMAL　機体の底を地表ともっと平行にすると SUCCESS です。";
         SetPhase(Phase::LandingPractice);
         break;
      case LandingResult::Failure:
         landingFeedback_ =
            "着地判定：FAILED　機体が大きく傾いています。姿勢を戻して再挑戦しましょう。";
         SetPhase(Phase::LandingPractice);
         break;
      default:
         break;
   }
}

std::string TutorialProgressComponent::BuildGuideText() const {
   switch (phase_) {
      case Phase::Steering:
         return
            "チュートリアル  1 / 4　走行と旋回\n"
            "機体は自動で前進します。A / D または左スティック左右で旋回してください。";
      case Phase::Jump:
         return
            "チュートリアル  2 / 4　ジャンプ\n"
            "正面の青い惑星へ向かい、SPACE または A ボタンでジャンプしてください。";
      case Phase::AirControl: {
         const std::string pitchStatus = usedPitch_ ? "OK" : "未操作";
         const std::string rollStatus = usedRoll_ ? "OK" : "未操作";
         return
            "チュートリアル  3 / 4　空中姿勢\n"
            "W / S・左スティック上下：ピッチ［" + pitchStatus +
            "］　Q / E・LB / RB：ロール［" + rollStatus + "］";
      }
      case Phase::PlanetTransfer: {
         const std::string retryText = landingFeedback_.empty()
            ? std::string()
            : "\n" + landingFeedback_;
         return
            "チュートリアル  4 / 4　惑星間ジャンプと着地\n"
            "青い惑星へ着地します。機体の上方向を地表の外側へそろえると SUCCESS です。" +
            retryText;
      }
      case Phase::LandingPractice:
         return
            "チュートリアル  4 / 4　成功着地を体験\n" + landingFeedback_ +
            "\nSPACE / A で再ジャンプし、空中で姿勢を整えて着地してください。";
      case Phase::Complete:
         return
            "TUTORIAL COMPLETE！　着地成功\n"
            "機体の底と地表が平行な状態が SUCCESS です。ゲームを開始します。";
      default:
         return {};
   }
}

const char* TutorialProgressComponent::GetPhaseName() const {
   switch (phase_) {
      case Phase::Steering: return "Steering";
      case Phase::Jump: return "Jump";
      case Phase::AirControl: return "AirControl";
      case Phase::PlanetTransfer: return "PlanetTransfer";
      case Phase::LandingPractice: return "LandingPractice";
      case Phase::Complete: return "Complete";
      default: return "Unknown";
   }
}

#ifdef USE_IMGUI
void TutorialProgressComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Player Object ID: %s", playerObjectId_.c_str());
   ImGui::Text("Next Scene: %s", nextScene_.c_str());
   ImGui::Text("Phase: %s", GetPhaseName());
   ImGui::DragInt("Target Planet Index", &targetPlanetIndex_, 1.0f, 0, 64);
   ImGui::DragFloat("Completion Delay", &completionDelay_, 0.1f, 0.0f, 10.0f, "%.1f s");
   ImGui::Text("Resolved: Jump=%s Landing=%s Switcher=%s Boost=%s Text=%s",
      characterJump_ ? "true" : "false",
      characterLanding_ ? "true" : "false",
      planetSwitcher_ ? "true" : "false",
      landingBoost_ ? "true" : "false",
      guideText_ ? "true" : "false");
}
#endif

} // namespace App
