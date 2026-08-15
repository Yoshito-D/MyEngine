#include "CameraModeSwitcher.h"

#include "CameraGravityBridge.h"
#include "GravityFollowCamera.h"
#include "PlanetLeashCamera.h"
#include "PlayerRearFollowCamera.h"
#include "Object/Object.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Scene/SceneWorld.h"
#include "../Vehicle/VehicleController.h"
#include "../Vehicle/VehicleInputComponent.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CameraModeSwitcher::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   cameras_.clear();
   cameras_.reserve(cameraIds_.size());
   // ID順を保持することで、シリアライズされた初期インデックスと入力による巡回順を一致させる。
   for (const auto& cameraId : cameraIds_) {
      cameras_.push_back(sceneWorld.FindVirtualCamera(cameraId));
   }
   currentIndex_ = cameras_.empty() ? 0 : std::min(initialIndex_, cameras_.size() - 1);
   ApplyMode();
}

void CameraModeSwitcher::Update(float) {
#ifdef MYPROJECT_NON_RELEASE
   const auto* vehicleInput = HasOwner() ? GetOwner().GetComponent<VehicleInputComponent>() : nullptr;
   if (cameras_.empty() || !vehicleInput || !vehicleInput->IsNextCameraTriggered()) {
      return;
   }
   currentIndex_ = (currentIndex_ + 1) % cameras_.size();
   ApplyMode();
#endif
}

bool CameraModeSwitcher::SwitchToCamera(const std::string& cameraId) {
   const auto cameraIt = std::find(cameraIds_.begin(), cameraIds_.end(), cameraId);
   if (cameraIt == cameraIds_.end()) {
      return false;
   }

   const size_t cameraIndex = static_cast<size_t>(std::distance(cameraIds_.begin(), cameraIt));
   if (cameraIndex >= cameras_.size() || !cameras_[cameraIndex]) {
      return false;
   }

   currentIndex_ = cameraIndex;
   ApplyMode();
   return true;
}

nlohmann::json CameraModeSwitcher::Serialize() const {
   return nlohmann::json{
      { "cameraIds", cameraIds_ },
      { "initialIndex", initialIndex_ }
   };
}

void CameraModeSwitcher::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("cameraIds") && data.at("cameraIds").is_array()) {
      cameraIds_.clear();
      for (const auto& cameraId : data.at("cameraIds")) {
         if (cameraId.is_string() && !cameraId.get<std::string>().empty()) {
            cameraIds_.push_back(cameraId.get<std::string>());
         }
      }
   }
   if (data.contains("initialIndex") && data.at("initialIndex").is_number_unsigned()) {
      initialIndex_ = data.at("initialIndex").get<size_t>();
   }
}

void CameraModeSwitcher::ApplyMode() {
   // Brainは優先度最大のカメラを選ぶため、選択対象だけを基準値へ上げる。
   for (size_t index = 0; index < cameras_.size(); ++index) {
      if (cameras_[index]) {
         cameras_[index]->SetPriority(index == currentIndex_ ? 0 : -1);
      }
   }
   if (!HasOwner() || currentIndex_ >= cameras_.size()) {
      return;
   }

   auto* selectedCamera = cameras_[currentIndex_];
   auto* gravityFollow = selectedCamera ? selectedCamera->GetComponent<GravityFollowCamera>() : nullptr;
   auto* rearFollow = selectedCamera ? selectedCamera->GetComponent<PlayerRearFollowCamera>() : nullptr;
   auto* planetLeash = selectedCamera ? selectedCamera->GetComponent<PlanetLeashCamera>() : nullptr;

   // 重力方向や車両姿勢を補正する側にも、Brainと同じアクティブカメラの部品を渡す。
   if (auto* bridge = GetOwner().GetComponent<CameraGravityBridge>()) {
      bridge->SetGravityFollowCamera(gravityFollow);
      bridge->SetPlayerRearFollowCamera(rearFollow);
      bridge->SetPlanetLeashCamera(planetLeash);
   }
   if (auto* controller = GetOwner().GetComponent<VehicleController>()) {
      controller->SetGravityFollowCamera(gravityFollow);
   }
}

#ifdef USE_IMGUI
void CameraModeSwitcher::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Camera: %zu / %zu", cameras_.empty() ? 0 : currentIndex_ + 1, cameras_.size());
}
#endif

} // namespace App
