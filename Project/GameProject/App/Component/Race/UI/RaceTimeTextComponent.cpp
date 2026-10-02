#include "GameProject/App/Component/Race/UI/RaceTimeTextComponent.h"

#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Race/UI/RaceTimeFormatting.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void RaceTimeTextComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
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
   GameEngine::EditorUI::ObjectReference("Race Manager", raceManagerId_, "RaceManagerComponent");
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
