#include "PlayerRearFollowCamera.h"
#include "RearCameraDebugView.h"
#include "Scene/Camera/Core/VirtualCamera.h"

namespace App {
namespace {
template<class T>
bool RegisterRearComponent(const char* name) {
   return GameEngine::VirtualCamera::RegisterComponentFactory(name,
      [](GameEngine::VirtualCamera& owner) -> GameEngine::ICinemachineComponent* {
         // 新しい部品名だけを読み込んだ場合も、入力と依存部品を一度だけ補う。
         if (!owner.GetComponent<PlayerRearFollowCamera>()) owner.AddComponent<PlayerRearFollowCamera>();
         if (auto* existing = owner.GetComponent<T>()) return existing;
         return owner.AddComponent<T>();
      });
}

const bool kRegisteredRearCameraTransition = RegisterRearComponent<RearCameraTransition>("RearCameraTransition");
const bool kRegisteredRearCameraGravityUp = RegisterRearComponent<RearCameraGravityUp>("RearCameraGravityUp");
const bool kRegisteredRearCameraPlanetGuide = RegisterRearComponent<RearCameraPlanetGuide>("RearCameraPlanetGuide");
const bool kRegisteredRearCameraDirectionTracker = RegisterRearComponent<RearCameraDirectionTracker>("RearCameraDirectionTracker");
const bool kRegisteredRearCameraSpeedEffects = RegisterRearComponent<RearCameraSpeedEffects>("RearCameraSpeedEffects");
const bool kRegisteredRearCameraPositionSolver = RegisterRearComponent<RearCameraPositionSolver>("RearCameraPositionSolver");
const bool kRegisteredRearCameraAimSolver = RegisterRearComponent<RearCameraAimSolver>("RearCameraAimSolver");
const bool kRegisteredRearCameraMeasurementRecorder = RegisterRearComponent<RearCameraMeasurementRecorder>("RearCameraMeasurementRecorder");
const bool kRegisteredRearCameraDebugView = RegisterRearComponent<RearCameraDebugView>("RearCameraDebugView");

} // namespace
} // namespace App
