#include "Object/Component/ComponentRegistry.h"
#include "Object/Object.h"

#include "Camera/CameraGravityBridge.h"
#include "Camera/ScreenSpaceBasis.h"
#include "Character/CharacterController.h"
#include "Character/CharacterJump.h"
#include "Character/CharacterLanding.h"
#include "Character/CharacterWalker.h"
#include "Gravity/GravityAttractorLink.h"
#include "Gravity/GravityBody.h"
#include "Gravity/MeshNormalGravityAttractor.h"
#include "Gravity/PlanetSwitcher.h"
#include "Gravity/SphericalGravityAttractor.h"
#include "Vehicle/VehicleAirController.h"
#include "Vehicle/VehicleController.h"
#include "Vehicle/VehicleDrift.h"
#include "Vehicle/VehicleGroundMover.h"
#include "Vehicle/VehicleLandingAligner.h"
#include "Vehicle/VehicleLandingBoost.h"
#include "Vehicle/VehicleMover.h"
#include "Vehicle/VehicleSpeedPostEffectController.h"

namespace {

template <typename T>
bool RegisterAppComponent() {
   return GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      T::kTypeName,
      [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
         return object.AddComponent<T>();
      },
      T::kDisplayName);
}

const bool kRegisteredAppComponents[] = {
   RegisterAppComponent<App::CameraGravityBridge>(),
   RegisterAppComponent<App::ScreenSpaceBasis>(),
   RegisterAppComponent<App::CharacterController>(),
   RegisterAppComponent<App::CharacterJump>(),
   RegisterAppComponent<App::CharacterLanding>(),
   RegisterAppComponent<App::CharacterWalker>(),
   RegisterAppComponent<App::GravityAttractorLink>(),
   RegisterAppComponent<App::GravityBody>(),
   RegisterAppComponent<App::MeshNormalGravityAttractor>(),
   RegisterAppComponent<App::PlanetSwitcher>(),
   RegisterAppComponent<App::SphericalGravityAttractor>(),
   RegisterAppComponent<App::VehicleAirController>(),
   RegisterAppComponent<App::VehicleController>(),
   RegisterAppComponent<App::VehicleDrift>(),
   RegisterAppComponent<App::VehicleGroundMover>(),
   RegisterAppComponent<App::VehicleLandingAligner>(),
   RegisterAppComponent<App::VehicleLandingBoost>(),
   RegisterAppComponent<App::VehicleMover>(),
   RegisterAppComponent<App::VehicleSpeedPostEffectController>(),
};

} // namespace
