#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
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

   const auto* jump = GetOwner().GetComponent<CharacterJump>();
   const auto* landing = GetOwner().GetComponent<CharacterLanding>();
   const auto* drift = GetOwner().GetComponent<VehicleDrift>();
   const auto* ground = GetOwner().GetComponent<VehicleGroundMover>();
   snapshot_.isAirborne = jump && jump->IsJumping();
   snapshot_.isGrounded = landing && landing->IsGrounded() && !snapshot_.isAirborne;
   snapshot_.groundSpeed = ground && ground->IsEnabled() ? ground->GetCurrentSpeed() : 0.0f;
   snapshot_.isDrifting = drift && drift->IsEnabled() && drift->IsDrifting();
   snapshot_.miniTurboReady = drift && drift->CanFireMiniTurbo();
}

void VehicleEvents::PublishTakeoff() {
   ++snapshot_.takeoffCount;
   snapshot_.isAirborne = true;
   snapshot_.isGrounded = false;
   snapshot_.hasTakeoffContact = false;

   if (!HasOwner()) return;

   const auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();
   const auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   GameEngine::Vector3 center;
   float radius = 0.0f;

   if (!switcher || !transform || !switcher->TryGetCurrentPlanet(center, radius)) return;

   // 積分後の車体位置や次の着地候補を使わず、飛び立つ地表の位置と法線を固定する。
   const auto world = transform->ComposeWorldMatrix(GameEngine::MakeAffineMatrix(transform->GetLocalPose()));
   const GameEngine::Vector3 position{world.m[3][0], world.m[3][1], world.m[3][2]};
   const auto normal = (position - center).Normalize();

   if (normal.LengthSquared() <= 0.0f) return;

   const auto* landing = GetOwner().GetComponent<CharacterLanding>();
   const float offset = landing ? landing->DescribeSettings().landingOffset : 0.0f;
   snapshot_.takeoffPoint = center + normal * (radius + offset);
   snapshot_.takeoffNormal = normal;
   snapshot_.hasTakeoffContact = true;
}

void VehicleEvents::PublishLanding(const GameEngine::Vector3& point, const GameEngine::Vector3& normal) {
   ++snapshot_.landingCount;
   snapshot_.landingPoint = point;
   snapshot_.landingNormal = normal;
   snapshot_.hasLandingContact = true;
   snapshot_.landingResult = LandingResult::Normal;
   snapshot_.isAirborne = false;
   snapshot_.isGrounded = true;
}

void VehicleEvents::SetLandingResult(LandingResult result) {
   snapshot_.landingResult = result;
}

void VehicleEvents::PublishMiniTurbo() {
   ++snapshot_.miniTurboCount;
}

void VehicleEvents::PublishDriftEnd() {
   ++snapshot_.driftEndCount;
}

} // namespace App
