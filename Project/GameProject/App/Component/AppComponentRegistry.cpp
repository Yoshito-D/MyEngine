#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Object/Object.h"

#include "GameProject/App/Component/Camera/CameraGravityBridge.h"
#include "GameProject/App/Component/Camera/CameraModeSwitcher.h"
#include "GameProject/App/Component/Camera/ScreenSpaceBasis.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Character/CharacterWalker.h"
#include "GameProject/App/Component/Gravity/GravityAttractorLink.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Gravity/MeshNormalGravityAttractor.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameProject/App/Component/Race/UI/GamepadGuideTextComponent.h"
#include "GameProject/App/Component/Race/RaceGateComponent.h"
#include "GameProject/App/Component/Race/UI/RaceCountdownTextComponent.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDistanceTextComponent.h"
#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Race/UI/RaceResultUIComponent.h"
#include "GameProject/App/Component/Race/UI/RaceTimeTextComponent.h"
#include "GameProject/App/Component/Race/UI/VehicleSpeedGaugeUIComponent.h"
#include "GameProject/App/Component/Title/TitleStartComponent.h"
#include "GameProject/App/Component/Tutorial/TutorialProgressComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleAirController.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleEffectController.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingAligner.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"
#include "GameProject/App/Component/Vehicle/VehicleSpeedPostEffectController.h"

namespace {

template <typename T>
bool RegisterAppComponent(GameEngine::ObjectTypeMask supportedObjectTypes = GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Model)) {
   // 各コンポーネントが公開する型名・表示名を共通のファクトリー形式へ束ねる。
   // デフォルトを Model に限定し、対応型を明示していないゲームプレイ機能が不適切な UI 等へ付くのを防ぐ。
   return GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      T::kTypeName,
      [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
         return object.AddComponent<T>();
      },
      T::kDisplayName,
      supportedObjectTypes);
}

// 静的初期化でゲーム固有型を登録し、シーン復元時に型名だけから生成できるようにする。
const bool kRegisteredAppComponents[] = {
   // UI 専用機能や管理オブジェクトは個別の型マスクを指定し、エディターの追加候補と JSON 復元を同じ制約に揃える。
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
   RegisterAppComponent<App::GamepadGuideTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceManagerComponent>(GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model),
   RegisterAppComponent<App::RaceGateComponent>(
      GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite),
   RegisterAppComponent<App::RaceCountdownTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceGoalDirectionHUDComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Model)),
   RegisterAppComponent<App::RaceGoalDistanceTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceResultUIComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::RaceTimeTextComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::VehicleSpeedGaugeUIComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Sprite)),
   RegisterAppComponent<App::TitleStartComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
   RegisterAppComponent<App::TutorialProgressComponent>(GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText)),
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
