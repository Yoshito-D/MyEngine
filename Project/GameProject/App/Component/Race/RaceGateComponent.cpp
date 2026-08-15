#include "RaceGateComponent.h"

#include "RaceManagerComponent.h"
#include "Object/Component/TriggerVolumeComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 140.0f;
#endif
}

namespace App {

void RaceGateComponent::OnAttach() {
   auto* trigger = GetOwner().AddComponent<GameEngine::TriggerVolumeComponent>();
   if (trigger && trigger->GetTargetObjectId().empty()) {
      // GameTestでは追加直後からゴールとして使え、他シーンではJSONから上書きできる既定値にする。
      trigger->SetTargetObjectId("Model:Player");
   }
}

void RaceGateComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceGateComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   const auto* trigger = GetOwner().GetComponent<GameEngine::TriggerVolumeComponent>();
   if (!trigger) {
      return;
   }

   // 接触中ではなく進入エッジだけを通知し、滞在中のチェックポイント連打を避ける。
   if (trigger->WasEnteredThisFrame()) {
      switch (gateType_) {
         case GateType::Start:
            raceManager_->NotifyStart();
            break;
         case GateType::Checkpoint:
            raceManager_->NotifyCheckpoint(checkpointIndex_);
            break;
         case GateType::Finish:
            raceManager_->NotifyFinish();
            break;
         case GateType::StartFinish:
            raceManager_->NotifyStartFinish();
            break;
      }
   }
   if (gateType_ == GateType::StartFinish && trigger->WasExitedThisFrame()) {
      // 共用ゲートは一度外へ出た事実を記録し、スタート直後の進入をゴールと区別する。
      raceManager_->NotifyStartGateExit();
   }
}

nlohmann::json RaceGateComponent::Serialize() const {
   const char* gateTypeName = "Finish";
   switch (gateType_) {
      case GateType::Start: gateTypeName = "Start"; break;
      case GateType::Checkpoint: gateTypeName = "Checkpoint"; break;
      case GateType::StartFinish: gateTypeName = "StartFinish"; break;
      default: break;
   }
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "gateType", gateTypeName },
      { "checkpointIndex", checkpointIndex_ }
   };
}

void RaceGateComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("gateType") && data.at("gateType").is_string()) {
      const std::string gateType = data.at("gateType").get<std::string>();
      if (gateType == "Start") {
         gateType_ = GateType::Start;
      } else if (gateType == "Checkpoint") {
         gateType_ = GateType::Checkpoint;
      } else if (gateType == "StartFinish") {
         gateType_ = GateType::StartFinish;
      } else {
         gateType_ = GateType::Finish;
      }
   }
   if (data.contains("checkpointIndex") && data.at("checkpointIndex").is_number_unsigned()) {
      checkpointIndex_ = data.at("checkpointIndex").get<size_t>();
   }
}

#ifdef USE_IMGUI
void RaceGateComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   GameEngine::ImGuiHelper::DrawInputString(
      "Race Manager ID",
      raceManagerId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   const char* gateTypes[] = { "Start", "Checkpoint", "Finish", "StartFinish" };
   int gateTypeIndex = static_cast<int>(gateType_);
   if (ImGui::Combo("Gate Type", &gateTypeIndex, gateTypes, 4)) {
      gateType_ = static_cast<GateType>(gateTypeIndex);
   }
   if (gateType_ == GateType::Checkpoint) {
      int checkpointIndex = static_cast<int>(checkpointIndex_);
      if (ImGui::DragInt("Checkpoint Index", &checkpointIndex, 1.0f, 0)) {
         checkpointIndex_ = static_cast<size_t>(std::max(checkpointIndex, 0));
      }
   }
   ImGui::TextDisabled("Trigger Volume is added automatically.");
}
#endif

} // namespace App
