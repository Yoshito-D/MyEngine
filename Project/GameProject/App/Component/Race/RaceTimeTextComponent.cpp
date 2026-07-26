#include "RaceTimeTextComponent.h"

#include "RaceManagerComponent.h"
#include "RaceTimeFormatting.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void RaceTimeTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceTimeTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   const RaceManagerComponent::State raceState = raceManager_->GetState();
   // カウントダウンとリザルトには専用UIがあるため、同じ位置で文字が重ならないよう隠す。
   if (raceState == RaceManagerComponent::State::Countdown ||
      raceState == RaceManagerComponent::State::Finished) {
      text->SetText("");
      return;
   }

   if (raceState == RaceManagerComponent::State::Waiting) {
      text->SetText("00:00.000");
      return;
   }

   text->SetText(FormatRaceTime(raceManager_->GetElapsedTime()));
}

nlohmann::json RaceTimeTextComponent::Serialize() const {
   return nlohmann::json{ { "raceManagerId", raceManagerId_ } };
}

void RaceTimeTextComponent::Deserialize(const nlohmann::json& data) {
   if (data.is_object() && data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
}

#ifdef USE_IMGUI
void RaceTimeTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
