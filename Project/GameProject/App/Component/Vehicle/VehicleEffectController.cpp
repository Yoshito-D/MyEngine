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

GameEngine::Quaternion AlignUpToNormal(const GameEngine::Vector3& normal) {
   const GameEngine::Vector3 localUp{ 0.0f, 1.0f, 0.0f };
   const GameEngine::Vector3 surfaceNormal = normal.Normalize();
   const float dot = localUp.Dot(surfaceNormal);

   // 真下だけは回転軸が一意に決まらないため、X軸周りの180度回転を使う。
   if (dot < -0.9999f) {
      return { 1.0f, 0.0f, 0.0f, 0.0f };
   }

   const GameEngine::Vector3 axis = localUp.Cross(surfaceNormal);
   return GameEngine::Quaternion{
      axis.x,
      axis.y,
      axis.z,
      1.0f + dot
   }.Normalize();
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

   GameEngine::Vector3 landingContactPoint{};
   GameEngine::Quaternion landingRotation = GameEngine::Quaternion::Identity();
   const bool hasLandingTransform =
	  landedThisFrame && landing && landing->HasLandingContact();
   if (hasLandingTransform) {
	  landingContactPoint = landing->GetLastLandingContactPoint();
	  landingRotation = AlignUpToNormal(landing->GetLastLandingNormal());
   }

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
         if (hasLandingTransform) {
            emitter->SetSlotWorldTransform(
               slotIndex,
               landingContactPoint,
               landingRotation,
               slot->attachConfig.scaleOffset);
         }
         emitter->Play(slotIndex);
      } else if (ContainsEffectName(slot->jsonPath, "landingDust") && landedThisFrame) {
         if (hasLandingTransform) {
            emitter->SetSlotWorldTransform(
               slotIndex,
               landingContactPoint,
               landingRotation,
               slot->attachConfig.scaleOffset);
         }
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
