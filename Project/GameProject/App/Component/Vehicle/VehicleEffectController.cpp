#include "GameProject/App/Component/Vehicle/VehicleEffectController.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>
#include <array>
#include <limits>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {
namespace {
constexpr std::array kRoleNames = {
   "DriftDust", "TurboReady", "TurboBurst", "TurboFlame", "Landing", "LandingFailure", "Jump"};

GameEngine::Quaternion AlignUpToNormal(const GameEngine::Vector3& normal) {
   const GameEngine::Vector3 localUp{0.0f, 1.0f, 0.0f};
   const auto surfaceNormal = normal.Normalize();
   const float dot = localUp.Dot(surfaceNormal);

   // 真下だけは回転軸が一意に決まらないため、X軸周りの180度回転を使う。
   if (dot < -0.9999f) {
      return {1.0f, 0.0f, 0.0f, 0.0f};
   }

   const auto axis = localUp.Cross(surfaceNormal);
   return GameEngine::Quaternion{axis.x, axis.y, axis.z, 1.0f + dot}.Normalize();
}
}

void VehicleEffectController::OnSceneLoaded(GameEngine::SceneWorld&) {
   const auto* events = HasOwner() ? GetOwner().GetComponent<VehicleEvents>() : nullptr;
   eventCursor_.Reset(events ? events->GetSnapshot() : VehicleEventSnapshot{});
}

void VehicleEffectController::Update(float) {
   if (!HasOwner()) return;

   auto* emitter = GetOwner().GetComponent<GameEngine::ParticleEmitterComponent>();
   const auto* events = GetOwner().GetComponent<VehicleEvents>();

   if (!emitter || !events) return;

   const auto& snapshot = events->GetSnapshot();
   const auto changes = eventCursor_.Read(snapshot);

   for (const auto& binding : bindings_) {
      const auto* slot = emitter->DescribeSlot(binding.slot);

      if (!slot) {
         continue;
      }

      switch (binding.role) {
         case VehicleEffectRole::DriftDust:
            emitter->EnableEmission(binding.slot, snapshot.isDrifting && !snapshot.isAirborne);
            break;
         case VehicleEffectRole::TurboReady:
            emitter->EnableEmission(binding.slot, snapshot.miniTurboReady && !snapshot.isAirborne);
            break;
         case VehicleEffectRole::TurboBurst:
            if (changes.miniTurbo) {
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::TurboFlame:
            if (!snapshot.isGrounded) {
               // 空中へ移った時点で既存粒子も消し、車体に追従して見え続けるのを防ぐ。
               emitter->EnableEmission(binding.slot, false);

               if (emitter->IsPlaying(binding.slot) || emitter->HasLivingParticles(binding.slot)) {
                  emitter->Stop(binding.slot);
               }
            } else if (changes.miniTurbo) {
               emitter->EnableEmission(binding.slot, true);
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::Landing:
            if (changes.landed) {
               if (snapshot.hasLandingContact) {
                  // 車体中心ではなく、確定済み接触点と法線へ着地演出を配置する。
                  emitter->SetSlotWorldTransform(binding.slot, snapshot.landingPoint,
                     AlignUpToNormal(snapshot.landingNormal), slot->attachConfig.scaleOffset);
               }

               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::LandingFailure:
            if (changes.landed && snapshot.landingResult == LandingResult::Failure) {
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::Jump:
            if (changes.tookOff) {
               emitter->Play(binding.slot);
            }
            break;
      }
   }
}

nlohmann::json VehicleEffectController::Serialize() const {
   auto bindings = nlohmann::json::array();

   for (const auto& binding : bindings_) {
      bindings.push_back({{"role", kRoleNames[static_cast<size_t>(binding.role)]}, {"slot", binding.slot}});
   }

   return {{"bindings", std::move(bindings)}};
}

void VehicleEffectController::Deserialize(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("bindings")) return;

   const auto& bindings = data.at("bindings");
   std::vector<VehicleEffectBinding> parsed;
   const auto fail = []() {
      Logger::Warning(
         "Vehicle effect bindings require known roles and non-negative slot indices.", Logger::LogChannel::Game);
   };

   if (!bindings.is_array()) {
      fail();
      return;
   }

   for (const auto& entry : bindings) {
      if (!entry.is_object() || !entry.contains("role") || !entry.at("role").is_string() || !entry.contains("slot") ||
          !entry.at("slot").is_number_integer()) {
         fail();
         return;
      }

      const auto roleName = entry.at("role").get<std::string>();
      const auto role = std::find(kRoleNames.begin(), kRoleNames.end(), roleName);
      const auto slot = entry.at("slot").get<std::int64_t>();

      if (role == kRoleNames.end() || slot < 0 || slot > std::numeric_limits<std::int32_t>::max()) {
         fail();
         return;
      }

      const VehicleEffectBinding binding{
         static_cast<VehicleEffectRole>(std::distance(kRoleNames.begin(), role)), static_cast<std::int32_t>(slot)};

      if (std::find(parsed.begin(), parsed.end(), binding) == parsed.end()) {
         parsed.push_back(binding);
      }
   }

   bindings_ = std::move(parsed);
}

#ifdef USE_IMGUI
void VehicleEffectController::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   for (size_t index = 0; index < bindings_.size(); ++index) {
      auto& binding = bindings_[index];
      ImGui::PushID(static_cast<std::int32_t>(index));
      auto role = static_cast<std::int32_t>(binding.role);

      if (ImGui::Combo("Role", &role, kRoleNames.data(), static_cast<std::int32_t>(kRoleNames.size()))) {
         binding.role = static_cast<VehicleEffectRole>(role);
      }

      ImGui::InputInt("Emitter Slot", &binding.slot);
      binding.slot = std::max(binding.slot, std::int32_t{0});
      const bool remove = ImGui::SmallButton("Remove");
      ImGui::PopID();

      if (remove) {
         bindings_.erase(bindings_.begin() + index);
         break;
      }
   }

   if (ImGui::Button("Add Effect Binding")) {
      bindings_.push_back({});
   }
}
#endif

} // namespace App
