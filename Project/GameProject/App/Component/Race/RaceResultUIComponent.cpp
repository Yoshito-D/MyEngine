#include "RaceResultUIComponent.h"

#include "RaceManagerComponent.h"
#include "RaceTimeFormatting.h"
#include "Framework/EngineContext.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <sstream>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void RaceResultUIComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceResultUIComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   if (raceManager_->GetState() != RaceManagerComponent::State::Finished) {
      text->SetText("");
      return;
   }

   text->SetText(BuildResultText());
   const auto& confirmAction =
      GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
   if (confirmAction.triggered) {
      raceManager_->RequestRestart();
   }
}

nlohmann::json RaceResultUIComponent::Serialize() const {
   return nlohmann::json{ { "raceManagerId", raceManagerId_ } };
}

void RaceResultUIComponent::Deserialize(const nlohmann::json& data) {
   if (data.is_object() && data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
}

std::string RaceResultUIComponent::BuildResultText() const {
   std::ostringstream stream;
   stream << "FINISH\nTIME " << FormatRaceTime(raceManager_->GetElapsedTime())
      << "\n\nBEST TIMES\n";

   const auto& bestTimes = raceManager_->GetBestTimes();
   for (size_t index = 0; index < 3; ++index) {
      stream << index + 1 << ". ";
      if (index < bestTimes.size()) {
         stream << FormatRaceTime(bestTimes[index]);
      } else {
         stream << "--:--.---";
      }
      stream << '\n';
   }
   stream << "\n> RESTART\nA / SPACE";
   return stream.str();
}

#ifdef USE_IMGUI
void RaceResultUIComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
