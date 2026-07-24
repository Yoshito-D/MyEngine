#include "VehicleInputComponent.h"

#include "Core/Input/InputActionService.h"
#include "Framework/EngineContext.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {

constexpr const char* kGameplayActionMap = "Gameplay";
constexpr const char* kSteerAction = "Vehicle.Steer";
constexpr const char* kPitchAction = "Vehicle.Pitch";
constexpr const char* kRollAction = "Vehicle.Roll";
constexpr const char* kJumpAction = "Vehicle.Jump";
constexpr const char* kDriftAction = "Vehicle.Drift";
constexpr const char* kCameraLookAction = "Camera.Look";
constexpr const char* kNextCameraAction = "Camera.Next";

} // namespace

const GameEngine::InputActionState& VehicleInputComponent::GetActionState(const char* actionId) const {
   static const GameEngine::InputActionState emptyState{};
   if (!IsEnabled()) {
	  return emptyState;
   }
   return GameEngine::EngineContext::GetInputActionState(kGameplayActionMap, actionId, playerSlot);
}

float VehicleInputComponent::GetSteerInput() const {
   return GetActionState(kSteerAction).value.x;
}

float VehicleInputComponent::GetPitchInput() const {
   return GetActionState(kPitchAction).value.x;
}

float VehicleInputComponent::GetRollInput() const {
   return GetActionState(kRollAction).value.x;
}

GameEngine::Vector2 VehicleInputComponent::GetCameraLookInput() const {
   return GetActionState(kCameraLookAction).value;
}

bool VehicleInputComponent::IsJumpTriggered() const {
   return GetActionState(kJumpAction).triggered;
}

bool VehicleInputComponent::IsDriftHeld() const {
   return GetActionState(kDriftAction).held;
}

bool VehicleInputComponent::IsNextCameraTriggered() const {
   return GetActionState(kNextCameraAction).triggered;
}

nlohmann::json VehicleInputComponent::Serialize() const {
   return nlohmann::json{ { "playerSlot", playerSlot } };
}

void VehicleInputComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("playerSlot") || !data.at("playerSlot").is_number_unsigned()) {
	  return;
   }
   const uint64_t configuredSlot = data.at("playerSlot").get<uint64_t>();
   playerSlot = static_cast<uint32_t>(std::min(
	  configuredSlot,
	  static_cast<uint64_t>(GameEngine::InputActionService::kMaxPlayers - 1)));
}

#ifdef USE_IMGUI
void VehicleInputComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
	  return;
   }

   int slot = static_cast<int>(playerSlot);
   if (ImGui::SliderInt(
	  GameEngine::LocalizeEditorText("プレイヤースロット", "Player Slot"),
	  &slot,
	  0,
	  static_cast<int>(GameEngine::InputActionService::kMaxPlayers - 1))) {
	  playerSlot = static_cast<uint32_t>(slot);
   }
}
#endif

} // namespace App
