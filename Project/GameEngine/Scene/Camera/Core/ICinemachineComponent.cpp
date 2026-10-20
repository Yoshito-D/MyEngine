#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"

namespace GameEngine {

void ICinemachineComponent::Attach(VirtualCamera& owner) {
   owner_ = &owner;
   OnAttach();
}

} // namespace GameEngine
