#include "Object/Component/ComponentRegistry.h"
#include "Object/Object.h"

#include "Camera/CameraGravityBridge.h"
#include "Camera/CameraModeSwitcher.h"
#include "Camera/ScreenSpaceBasis.h"
#include "Character/CharacterJump.h"
#include "Character/CharacterLanding.h"
#include "Character/CharacterWalker.h"
#include "Gravity/GravityAttractorLink.h"
#include "Gravity/GravityBody.h"
#include "Gravity/MeshNormalGravityAttractor.h"
#include "Gravity/PlanetSwitcher.h"
#include "Gravity/SphericalGravityAttractor.h"
#include "Race/RaceGateComponent.h"
#include "Race/RaceCountdownTextComponent.h"
#include "Race/RaceManagerComponent.h"
#include "Race/RaceResultUIComponent.h"
#include "Race/RaceTimeTextComponent.h"
#include "Vehicle/VehicleAirController.h"
#include "Vehicle/VehicleController.h"
#include "Vehicle/VehicleDrift.h"
#include "Vehicle/VehicleEffectController.h"
#include "Vehicle/VehicleGroundMover.h"
#include "Vehicle/VehicleInputComponent.h"
#include "Vehicle/VehicleLandingAligner.h"
#include "Vehicle/VehicleLandingBoost.h"
#include "Vehicle/VehicleMover.h"
#include "Vehicle/VehicleSpeedPostEffectController.h"

namespace {

template <typename T>
bool RegisterAppComponent(GameEngine::ObjectTypeMask supportedObjectTypes = GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Model)) {
   return GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      T::kTypeName,
      [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
         return object.AddComponent<T>();
      },
      T::kDisplayName,
      supportedObjectTypes);
}

const bool kRegisteredAppComponents[] = {
   RegisterAppComponent<App::CameraGravityBridge>(),
   RegisterAppComponent<App::CameraModeSwitcher>(),
   RegisterAppComponent<App::ScreenSpaceBasis>(),
   RegisterAppComponent<App::CharacterJump>(),
   RegisterAppComponent<App::CharacterLanding>(),
   RegisterAppComponent<App::CharacterWalker>(),
   RegisterAppComponent<App::GravityAttractorLink>(),
   RegisterAppComponent<App::GravityBody>(),
   RegisterAppComponent<App::MeshNormalGravityAttractor>(),
   RegisterAppComponent<App::PlanetSwitcher>(),
   RegisterAppComponent<App::SphericalGravityAttractor>(),
   RegisterAppComponent<App::RaceManagerComponent>(GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model),
   RegisterAppComponent<App::RaceGateComponent>(
      GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite),
   RegisterAppComponent<App::RaceCountdownTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceResultUIComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceTimeTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::VehicleAirController>(),
   RegisterAppComponent<App::VehicleController>(),
   RegisterAppComponent<App::VehicleDrift>(),
   RegisterAppComponent<App::VehicleEffectController>(),
   RegisterAppComponent<App::VehicleGroundMover>(),
   RegisterAppComponent<App::VehicleInputComponent>(),
   RegisterAppComponent<App::VehicleLandingAligner>(),
   RegisterAppComponent<App::VehicleLandingBoost>(),
   RegisterAppComponent<App::VehicleMover>(),
   RegisterAppComponent<App::VehicleSpeedPostEffectController>(),
};

} // namespace
