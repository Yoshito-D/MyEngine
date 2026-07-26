#include "RaceGoalDistanceTextComponent.h"

#include "RaceManagerComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>
#include <format>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {

#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 150.0f;
#endif

GameEngine::Vector3 GetWorldPosition(const GameEngine::Object& object) {
   const GameEngine::Matrix4x4 worldMatrix = object.GetWorldMatrix();
   return { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
}

} // namespace

namespace App {

void RaceGoalDistanceTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   playerObject_ = nullptr;
   goalObject_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);

   if (HasOwner()) {
      if (auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>()) {
         text->SetText("");
      }
   }
}

void RaceGoalDistanceTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   const bool shouldShow = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Running &&
      playerObject_ && goalObject_;
   if (!shouldShow) {
      text->SetText("");
      return;
   }

   const float distanceMeters =
      (GetWorldPosition(*goalObject_) - GetWorldPosition(*playerObject_)).Length();
   if (!std::isfinite(distanceMeters)) {
      text->SetText("");
      return;
   }
   text->SetText(std::format("{:.0f}m", std::max(distanceMeters, 0.0f)));
}

nlohmann::json RaceGoalDistanceTextComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ },
      { "goalObjectId", goalObjectId_ }
   };
}

void RaceGoalDistanceTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
   if (data.contains("goalObjectId") && data.at("goalObjectId").is_string()) {
      goalObjectId_ = data.at("goalObjectId").get<std::string>();
   }
}

#ifdef USE_IMGUI
void RaceGoalDistanceTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   GameEngine::ImGuiHelper::DrawInputString(
      "Race Manager ID",
      raceManagerId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString(
      "Player Object ID",
      playerObjectId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString(
      "Goal Object ID",
      goalObjectId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   ImGui::Text("Resolved: Race=%s Player=%s Goal=%s",
      raceManager_ ? "true" : "false",
      playerObject_ ? "true" : "false",
      goalObject_ ? "true" : "false");
}
#endif

} // namespace App
