#include "CameraModeSwitcher.h"

#include "GravityFollowCamera.h"
#include "Object/Object.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Scene/SceneWorld.h"
#include "../Vehicle/VehicleController.h"
#include "../Vehicle/VehicleInputComponent.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "Editor/EditorReferenceWidgets.h"
#include "ImguiManager.h"
#endif

namespace App {

void CameraModeSwitcher::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   cameras_.clear();
   cameras_.reserve(cameraIds_.size());
   // ID順を保持することで、シリアライズされた初期インデックスと入力による巡回順を一致させる。
   for (const auto& cameraId : cameraIds_) {
      cameras_.push_back(sceneWorld.FindVirtualCamera(cameraId));
   }
   currentIndex_ = cameras_.empty() ? 0 : std::min(currentIndex_, cameras_.size() - 1);
   if (HasOwner()) {
      if (auto* controller = GetOwner().GetComponent<VehicleController>()) {
         auto* selected = GetSelectedCamera();
         controller->SetGravityFollowCamera(selected ? selected->GetComponent<GravityFollowCamera>() : nullptr);
      }
   }
}

void CameraModeSwitcher::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
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
         if (cameraId.is_string()) {
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
   if (!HasOwner()) {
      return;
   }

   auto* selectedCamera = GetSelectedCamera();
   auto* gravityFollow = selectedCamera ? selectedCamera->GetComponent<GravityFollowCamera>() : nullptr;
   // Bridgeは非選択カメラにも入力を供給する。車両の操作基準だけを選択に合わせる。
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
   for (size_t index = 0; index < cameraIds_.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));
      GameEngine::EditorUI::CameraReference("Camera", cameraIds_[index]);
      if (ImGui::RadioButton("Initial Camera", initialIndex_ == index)) initialIndex_ = index;
      ImGui::BeginDisabled(index == 0);
      const bool up = ImGui::SmallButton("Up");
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(index + 1 == cameraIds_.size());
      const bool down = ImGui::SmallButton("Down");
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool remove = ImGui::SmallButton("Remove");
      ImGui::PopID();
      if (up || down) {
         const size_t other = up ? index - 1 : index + 1;
         std::swap(cameraIds_[index], cameraIds_[other]);
         if (initialIndex_ == index) initialIndex_ = other;
         else if (initialIndex_ == other) initialIndex_ = index;
         break;
      }
      if (remove) {
         cameraIds_.erase(cameraIds_.begin() + index);
         if (initialIndex_ > index) --initialIndex_;
         if (initialIndex_ >= cameraIds_.size()) initialIndex_ = 0;
         break;
      }
   }
   if (ImGui::Button("Add Camera")) cameraIds_.emplace_back();
   ImGui::Text("Camera: %zu / %zu", cameras_.empty() ? 0 : currentIndex_ + 1, cameras_.size());
}
#endif

} // namespace App
