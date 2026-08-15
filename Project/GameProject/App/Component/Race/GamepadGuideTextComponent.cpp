#include "GamepadGuideTextComponent.h"

#include "RaceManagerComponent.h"
#include "../Character/CharacterJump.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {

constexpr const char* kGroundGuideText =
   "左スティック：旋回 / ドリフト\n"
   "A：ジャンプ";

constexpr const char* kAirGuideText =
   "左スティック上下：ピッチ\n"
   "LB / RB：ロール";

} // namespace

void GamepadGuideTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   characterJump_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      characterJump_ = playerObject->GetComponent<CharacterJump>();
   }
}

void GamepadGuideTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   const bool isShowingResult = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Finished;
   if (isShowingResult) {
      text->SetText("");
      return;
   }

   text->SetText(characterJump_ && characterJump_->IsJumping()
      ? kAirGuideText
      : kGroundGuideText);
}

nlohmann::json GamepadGuideTextComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ }
   };
}

void GamepadGuideTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
}

#ifdef USE_IMGUI
void GamepadGuideTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Player Object ID: %s", playerObjectId_.c_str());
   ImGui::Text("Resolved: Race=%s Player Jump=%s",
      raceManager_ ? "true" : "false",
      characterJump_ ? "true" : "false");
}
#endif

} // namespace App
