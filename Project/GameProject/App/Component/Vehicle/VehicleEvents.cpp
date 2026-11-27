#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"

namespace App {

void VehicleEventCursor::Reset(const VehicleEventSnapshot& snapshot) {
   takeoff_ = snapshot.takeoffCount;
   landing_ = snapshot.landingCount;
   miniTurbo_ = snapshot.miniTurboCount;
   driftEnd_ = snapshot.driftEndCount;
}

VehicleEventChanges VehicleEventCursor::Read(const VehicleEventSnapshot& snapshot) {
   const VehicleEventChanges changes{snapshot.takeoffCount > takeoff_, snapshot.landingCount > landing_,
      snapshot.miniTurboCount > miniTurbo_, snapshot.driftEndCount > driftEnd_};
   Reset(snapshot);
   return changes;
}

void VehicleEvents::OnSceneLoaded(GameEngine::SceneWorld&) {
   snapshot_ = {};
   Update(0.0f);
}

void VehicleEvents::Update(float) {
   if (!HasOwner()) return;

   const auto* landing = GetOwner().GetComponent<CharacterLanding>();
   const auto* drift = GetOwner().GetComponent<VehicleDrift>();
   const auto* ground = GetOwner().GetComponent<VehicleGroundMover>();
   snapshot_.isGrounded = landing && landing->IsGrounded();
   snapshot_.isAirborne = !snapshot_.isGrounded;
   snapshot_.groundSpeed = ground && ground->IsEnabled() ? ground->GetCurrentSpeed() : 0.0f;
   snapshot_.isDrifting = drift && drift->IsEnabled() && drift->IsDrifting();
   snapshot_.miniTurboReady = drift && drift->CanFireMiniTurbo();
}

void VehicleEvents::PublishTakeoff(const SurfaceContact& contact) {
   snapshot_.takeoffPoint = contact.point;
   snapshot_.takeoffNormal = contact.normal;
   snapshot_.hasTakeoffContact = true;
   snapshot_.isAirborne = true;
   snapshot_.isGrounded = false;
   ++snapshot_.takeoffCount;
}

void VehicleEvents::PublishLanding(const SurfaceContact& contact, LandingResult result) {
   snapshot_.landingPoint = contact.point;
   snapshot_.landingNormal = contact.normal;
   snapshot_.hasLandingContact = true;
   snapshot_.landingResult = result;
   snapshot_.isAirborne = false;
   snapshot_.isGrounded = true;
   ++snapshot_.landingCount;
}

void VehicleEvents::PublishMiniTurbo() {
   ++snapshot_.miniTurboCount;
}

void VehicleEvents::PublishDriftEnd() {
   ++snapshot_.driftEndCount;
}

} // namespace App
