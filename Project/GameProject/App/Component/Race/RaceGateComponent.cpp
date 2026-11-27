#include "GameProject/App/Component/Race/RaceGateComponent.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>

#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameEngine/Object/Component/Collision/TriggerVolumeComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#endif

namespace App {

void RaceGateComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   const auto& owner = GetOwner();
   const auto* trigger = owner.GetComponent<GameEngine::TriggerVolumeComponent>();

   if (!raceManager_) {
      Logger::Fail("Race gate " + owner.GetEntityId() + " cannot resolve raceManagerId " + raceManagerId_,
         Logger::LogChannel::Game);
   }

   // 形状と対象はシーンのTriggerVolumeで設定し、ゲートは進入・退出の通知だけを変換する。
   if (!trigger || !sceneWorld.FindObjectById(trigger->GetTargetObjectId())) {
      Logger::Fail("Race gate " + owner.GetEntityId() + " requires a TriggerVolume with a scene target",
         Logger::LogChannel::Game);
   }
}

void RaceGateComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み後に以前のRaceManagerを参照しないよう、保存済みIDから解決し直す。
   raceManager_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceGateComponent::Update(float deltaTime) {
   (void)deltaTime;

   if (!raceManager_ || !raceManager_->IsEnabled() || !HasOwner()) return;

   const auto* trigger = GetOwner().GetComponent<GameEngine::TriggerVolumeComponent>();

   // 無効化中はTriggerVolumeが更新されず、最後の進入・退出フラグが残る。
   if (!trigger || !trigger->IsEnabled()) return;

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
   // enumの整数値ではなく安定した名前を保存し、列挙順の変更で既存シーンの意味が変わらないようにする。
   const char* gateTypeName = "Finish";

   switch (gateType_) {
      case GateType::Start:
         gateTypeName = "Start";
         break;
      case GateType::Checkpoint:
         gateTypeName = "Checkpoint";
         break;
      case GateType::StartFinish:
         gateTypeName = "StartFinish";
         break;
      default:
         break;
   }

   return nlohmann::json{
      {"raceManagerId", raceManagerId_}, {"gateType", gateTypeName}, {"checkpointIndex", checkpointIndex_}};
}

nlohmann::json RaceGateComponent::GetSharedParameters() const {
   return ExtractSharedParameters({"raceManagerId", "checkpointIndex"});
}

void RaceGateComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void RaceGateComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void RaceGateComponent::ReadParameters(const nlohmann::json& data) {
   if (!data.is_object()) {
      Logger::Fail("RaceGateComponent settings must be a JSON object", Logger::LogChannel::Game);
   }

   const auto managerId = GameEngine::JsonValue::ReadString(data, "raceManagerId", raceManagerId_);
   auto gateType = gateType_;
   auto checkpointIndex = checkpointIndex_;

   if (data.contains("gateType")) {
      const auto name = GameEngine::JsonValue::ReadString(data, "gateType", "");

      if (name == "Start") {
         gateType = GateType::Start;
      } else if (name == "Checkpoint") {
         gateType = GateType::Checkpoint;
      } else if (name == "Finish") {
         gateType = GateType::Finish;
      } else if (name == "StartFinish") {
         gateType = GateType::StartFinish;
      } else {
         Logger::Fail("Invalid RaceGateComponent.gateType: " + name, Logger::LogChannel::Game);
      }
   }

   if (const auto index = data.find("checkpointIndex"); index != data.end()) {
      // 負値をsize_tへ変換すると巨大なゲート番号になるため、変換前に検証する。
      if (!index->is_number_integer() || (!index->is_number_unsigned() && index->get<std::int64_t>() < 0)) {
         Logger::Fail("Invalid RaceGateComponent.checkpointIndex", Logger::LogChannel::Game);
      }

      checkpointIndex = index->get<size_t>();
   }

   raceManagerId_ = managerId;
   gateType_ = gateType;
   checkpointIndex_ = checkpointIndex;
}

#ifdef USE_IMGUI
void RaceGateComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   GameEngine::EditorUI::ObjectReference("Race Manager", raceManagerId_, "RaceManagerComponent");
   const char* gateTypes[] = {"Start", "Checkpoint", "Finish", "StartFinish"};
   std::int32_t gateTypeIndex = static_cast<std::int32_t>(gateType_);

   if (ImGui::Combo("Gate Type", &gateTypeIndex, gateTypes, 4)) {
      gateType_ = static_cast<GateType>(gateTypeIndex);
   }

   if (gateType_ == GateType::Checkpoint) {
      std::int32_t checkpointIndex = static_cast<std::int32_t>(checkpointIndex_);

      if (ImGui::DragInt("Checkpoint Index", &checkpointIndex, 1.0f, 0)) {
         checkpointIndex_ = static_cast<size_t>(std::max(checkpointIndex, 0));
      }
   }

   ImGui::TextDisabled("Trigger Volume is added automatically.");
}
#endif

} // namespace App
