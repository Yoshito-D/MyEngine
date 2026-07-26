#include "RaceCountdownTextComponent.h"

#include "RaceManagerComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void RaceCountdownTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceCountdownTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   if (raceManager_->GetState() == RaceManagerComponent::State::Countdown) {
      // 切り上げにより残り時間が正の間は0を表示せず、GO表示との境界を明確にする。
      const int count = std::max(1, static_cast<int>(std::ceil(raceManager_->GetCountdownRemaining())));
      text->SetText(std::to_string(count));
   } else if (raceManager_->IsStartBannerVisible()) {
      text->SetText(startText_);
   } else {
      text->SetText("");
   }
}

nlohmann::json RaceCountdownTextComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "startText", startText_ }
   };
}

void RaceCountdownTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("startText") && data.at("startText").is_string()) {
      startText_ = data.at("startText").get<std::string>();
   }
}

#ifdef USE_IMGUI
void RaceCountdownTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
