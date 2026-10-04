#include "GameEngine/Scene/Camera/CameraManager.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/Camera/Camera.h"

namespace GameEngine {

CameraManager::CameraManager() = default;
CameraManager::~CameraManager() = default;

CinemachineBrain* CameraManager::CreateUnit(std::unique_ptr<Camera> outputCamera) {
   if (!outputCamera) return nullptr;
   auto unit = std::make_unique<CameraUnit>();
   unit->brain = std::make_unique<CinemachineBrain>();
   unit->brain->Initialize(std::move(outputCamera));

   CameraUnit* ptr = unit.get();
   units_.push_back(std::move(unit));

   if (!activeUnit_) {
	  // 明示選択前でもカメラAPIが利用できるよう、最初のユニットを既定出力にする。
	  activeUnit_ = ptr;
   }
   return ptr->brain.get();
}

CinemachineBrain* CameraManager::GetActiveBrain() {
   if (!activeUnit_) return nullptr;
   return activeUnit_->brain.get();
}

Camera* CameraManager::GetActiveCamera() {
   CinemachineBrain* brain = GetActiveBrain();
   if (!brain) return nullptr;
   return brain->GetOutputCamera();
}

const CinemachineBrain* CameraManager::GetActiveBrain() const {
   return activeUnit_ ? activeUnit_->brain.get() : nullptr;
}
const Camera* CameraManager::GetActiveCamera() const {
   const auto* brain = GetActiveBrain();
   return brain ? brain->GetOutputCamera() : nullptr;
}

void CameraManager::ClearUnits() {
   units_.clear();
   activeUnit_ = nullptr;
}
}
