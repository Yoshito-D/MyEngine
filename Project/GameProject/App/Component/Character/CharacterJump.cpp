#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include "GameEngine/Object/Object.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void CharacterJump::Jump() {
   if (!HasOwner()) return;

   auto* landing = GetOwner().GetComponent<CharacterLanding>();
   auto* body = GetOwner().GetComponent<GravityBody>();
   if (!landing || !landing->IsEnabled() || !body || !body->IsEnabled() || jumpStrength <= 0.0f) return;

   const auto contact = landing->Takeoff();
   if (!contact) return;

   body->AddVelocityImpulse(contact->normal * jumpStrength);

   if (auto* drift = GetOwner().GetComponent<VehicleDrift>()) {
      drift->ResetMiniTurboCharge();
   }
   if (auto* events = GetOwner().GetComponent<VehicleEvents>(); events && events->IsEnabled()) {
      events->PublishTakeoff(*contact);
   }
}

#ifdef USE_IMGUI
void CharacterJump::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::DragFloat(Tr("ジャンプ力 (m/s)", "Jump Strength (m/s)"), &jumpStrength, 0.1f, 0.0f, 30.0f);
   Configure(DescribeSettings());
}
#endif

nlohmann::json CharacterJump::Serialize() const {
   nlohmann::json json;
   json["jumpStrength"] = jumpStrength;
   return json;
}

nlohmann::json CharacterJump::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void CharacterJump::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void CharacterJump::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void CharacterJump::ReadParameters(const nlohmann::json& data) {
   if (data.contains("jumpStrength")) {
      jumpStrength = data["jumpStrength"];
   }

   Configure(DescribeSettings());
}

} // namespace App

namespace App {

CharacterJump::Settings CharacterJump::DescribeSettings() const {
   Settings settings;
   settings.jumpStrength = jumpStrength;
   return settings;
}

void CharacterJump::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.jumpStrength)) {
      settings.jumpStrength = defaults.jumpStrength;
   }

   jumpStrength = settings.jumpStrength;
}

} // namespace App
