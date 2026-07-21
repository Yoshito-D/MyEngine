#include "VehicleEffectController.h"

#include "VehicleDrift.h"
#include "../Character/CharacterJump.h"
#include "../Character/CharacterLanding.h"
#include "Effect/Module/EmissionModule.h"
#include "Effect/ParticleSystem.h"
#include "Object/Component/Particle/ParticleEmitterComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {
bool ContainsEffectName(const std::string& path, const char* effectName) {
   return path.find(effectName) != std::string::npos;
}

void SetEmission(GameEngine::ParticleEmitterComponent::EmitterSlot& slot, bool enabled) {
   if (slot.particleSystem) {
      if (auto* emission = slot.particleSystem->GetEmissionModule()) {
         emission->SetEnabled(enabled);
      }
   }
}
}

void VehicleEffectController::Update(float deltaTime) {
   (void)deltaTime;
   if (!HasOwner()) {
      return;
   }
   auto* emitter = GetOwner().GetComponent<GameEngine::ParticleEmitterComponent>();
   auto* drift = GetOwner().GetComponent<VehicleDrift>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();
   auto* landing = GetOwner().GetComponent<CharacterLanding>();
   if (!emitter) {
      return;
   }

   const bool isJumping = jump && jump->IsJumping();
   const bool isGrounded = landing && landing->IsGrounded();
   const bool isDrifting = drift && drift->IsDrifting();
   const bool canFireMiniTurbo = drift && drift->CanFireMiniTurbo();
   const bool miniTurboFired = drift && drift->ConsumeMiniTurboFired();
   const bool jumpedThisFrame = isJumping && !wasJumping_;
   const bool landedThisFrame = isGrounded && !wasGrounded_;

   for (int slotIndex = 0; slotIndex < emitter->GetSlotCount(); ++slotIndex) {
      auto* slot = emitter->GetSlot(slotIndex);
      if (!slot) {
         continue;
      }
      if (ContainsEffectName(slot->jsonPath, "tire_dust")) {
         SetEmission(*slot, isDrifting && !isJumping);
      } else if (ContainsEffectName(slot->jsonPath, "miniturbo")) {
         SetEmission(*slot, canFireMiniTurbo && !isJumping);
      } else if (ContainsEffectName(slot->jsonPath, "sonicBoom") && miniTurboFired) {
         emitter->Play(slotIndex);
      } else if (ContainsEffectName(slot->jsonPath, "bonfire") && miniTurboFired && !isJumping) {
         SetEmission(*slot, true);
         emitter->Play(slotIndex);
      } else if (ContainsEffectName(slot->jsonPath, "landingRing") && landedThisFrame) {
         emitter->Play(slotIndex);
      } else if (ContainsEffectName(slot->jsonPath, "landingDust") && landedThisFrame) {
         emitter->Play(slotIndex);
      } else if (ContainsEffectName(slot->jsonPath, "/jump.json") && jumpedThisFrame) {
         emitter->Play(slotIndex);
      }
   }

   wasJumping_ = isJumping;
   wasGrounded_ = isGrounded;
}

#ifdef USE_IMGUI
void VehicleEffectController::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Was Jumping: %s", wasJumping_ ? "true" : "false");
   ImGui::Text("Was Grounded: %s", wasGrounded_ ? "true" : "false");
}
#endif

} // namespace App
