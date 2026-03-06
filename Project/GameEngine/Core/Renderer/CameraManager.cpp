#include "CameraManager.h"

namespace GameEngine {
void CameraManager::AddCamera(Camera* camera) {
   if (camera) {
	  cameras_.push_back(camera);
	  if (!activeCamera_) { activeCamera_ = camera; }
   }
}

void CameraManager::SetActiveCamera(size_t index) {
   if (index < cameras_.size()) { activeCamera_ = cameras_[index]; }
}
}
