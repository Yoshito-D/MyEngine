#include "CameraManager.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Camera.h"

namespace GameEngine {

CameraManager::CameraManager() = default;
CameraManager::~CameraManager() = default;

CameraUnit* CameraManager::CreateUnit() {
   auto unit = std::make_unique<CameraUnit>();
   unit->brain = std::make_unique<CinemachineBrain>();

   CameraUnit* ptr = unit.get();
   units_.push_back(std::move(unit));

   if (!activeUnit_) {
	  activeUnit_ = ptr;
   }
   return ptr;
}

CinemachineBrain* CameraManager::GetActiveBrain() const {
   if (!activeUnit_) return nullptr;
   return activeUnit_->brain.get();
}

Camera* CameraManager::GetActiveCamera() const {
   CinemachineBrain* brain = GetActiveBrain();
   if (!brain) return nullptr;
   return brain->GetOutputCamera();
}
}
