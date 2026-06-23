#include "PlanetSwitcher.h"
#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterWalker.h"
#include "../Camera/CameraGravityBridge.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <Model/Model.h>
#include "SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

namespace {
constexpr const char* kSceneObjectPayload = "EDITOR_SCENE_OBJECT";

GameEngine::Model* FindRegisteredModelByObjectName(const std::string& objectName) {
   const auto& models = GameEngine::Model::GetRegisteredModels();
   auto it = std::find_if(models.begin(), models.end(),
      [&objectName](const GameEngine::Model* model) {
         return model && model->GetObjectName() == objectName;
      });
   return it != models.end() ? *it : nullptr;
}

nlohmann::json SerializeVector3(const GameEngine::Vector3& value) {
   return {
      { "x", value.x },
      { "y", value.y },
      { "z", value.z }
   };
}

GameEngine::Vector3 DeserializeVector3(const nlohmann::json& data, const GameEngine::Vector3& fallback) {
   if (!data.is_object()) {
      return fallback;
   }

   GameEngine::Vector3 value = fallback;
   if (data.contains("x") && data.at("x").is_number()) { value.x = data.at("x").get<float>(); }
   if (data.contains("y") && data.at("y").is_number()) { value.y = data.at("y").get<float>(); }
   if (data.contains("z") && data.at("z").is_number()) { value.z = data.at("z").get<float>(); }
   return value;
}
}

/// @brief OBBと点の最短距離を返す
/// @param point  ワールド空間の点
/// @param obbCenter OBB中心（ワールド）
/// @param obbRot    OBBの向き（クォータニオン）
/// @param halfExtents OBBの半サイズ (scale * 0.5f)
static float DistancePointOBB(
   const GameEngine::Vector3& point,
   const GameEngine::Vector3& obbCenter,
   const GameEngine::Quaternion& obbRot,
   const GameEngine::Vector3& halfExtents)
{
   // OBBローカル空間へ変換（逆回転）
   GameEngine::Quaternion invRot = obbRot.Inverse();
   GameEngine::Vector3 localVec = RotateVector(point - obbCenter, invRot);

   // 各軸でクランプして最近傍点を求め、距離を返す
   float dx = localVec.x - std::clamp(localVec.x, -halfExtents.x, halfExtents.x);
   float dy = localVec.y - std::clamp(localVec.y, -halfExtents.y, halfExtents.y);
   float dz = localVec.z - std::clamp(localVec.z, -halfExtents.z, halfExtents.z);
   return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void PlanetSwitcher::Update(float /*deltaTime*/) {
   // オーナー不在または候補未登録なら何もしない
   if (!HasOwner() || entries_.empty()) { return; }

   // 現在位置を取得
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }
   const GameEngine::Vector3 pos = transform->transform.translation;

   // OBBパラメータを取得
   const GameEngine::Quaternion obbRot = transform->transform.GetActiveQuaternion();
   const GameEngine::Vector3    halfExtents = obbHalfExtents;

   // 影響圏内の最近傍惑星を探索（OBB最近傍点距離を使用）
   int bestInRange = -1;
   float bestInRangeDist = std::numeric_limits<float>::max();
   int bestAnyIndex = -1;
   float bestAnyDist = std::numeric_limits<float>::max();

   for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
	  auto& e = entries_[i];
	  if (e.objectName.empty()) { continue; }

	  // 位置と半径を更新
	  e.center = GetPlanetCenter(e.objectName);
	  e.surfaceRadius = GetPlanetSurfaceRadius(e.objectName);

	  // OBBと惑星中心の最短距離（OBBの最近傍点→惑星中心）
	  float dist = DistancePointOBB(e.center, pos, obbRot, halfExtents);

	  if (dist < bestAnyDist) {
		 bestAnyDist = dist;
		 bestAnyIndex = i;
	  }

	  auto* attractor = GetAttractorByObjectName(e.objectName);
	  if (attractor && attractor->IsInRange(pos) && dist < bestInRangeDist) {
		 bestInRangeDist = dist;
		 bestInRange = i;
	  }
   }

   if (bestAnyIndex < 0) {
	  return;
   }

   int newIndex = (bestInRange >= 0) ? bestInRange : bestAnyIndex;

   // ヒステリシス：現在の惑星への距離が新候補よりswitchHysteresis以上大きいときのみ切替
   if (newIndex != currentIndex_ && currentIndex_ >= 0 && currentIndex_ < static_cast<int>(entries_.size())) {
	  float currentDist = DistancePointOBB(entries_[currentIndex_].center, pos, obbRot, halfExtents);
	  float newDist = (newIndex == bestInRange) ? bestInRangeDist : bestAnyDist;
	  if (currentDist - newDist < switchHysteresis) {
		 newIndex = currentIndex_; // 差が不十分なので切り替えしない
	  }
   }

   const bool planetChanged = (newIndex != currentIndex_);
   if (planetChanged) {
	  currentIndex_ = newIndex;
	  const auto& best = entries_[currentIndex_];

	  // GravityAttractorLink の接続先は惑星切替時だけ更新する
	  if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
		 link->SetAttractor(GetAttractorByObjectName(best.objectName));
	  }

	  if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
		 walker->ResetHorizontalVelocity();
	  }

	  switched_ = true;
   }

   ApplyCurrentPlanetParameters();
}

void PlanetSwitcher::AddPlanet(std::string objectName) {
   if (objectName.empty() || HasPlanet(objectName)) {
	  return;
   }

   if (auto* model = FindRegisteredModelByObjectName(objectName)) {
	  GameEngine::Vector3 modelPos = model->GetPosition();
	  float surfaceRadius = model->GetScale().x; // 仮にスケールのX軸を半径として使用
	  entries_.push_back({ objectName, modelPos, surfaceRadius });
   }
}

void PlanetSwitcher::ApplyCurrentPlanetParameters() {
   if (!HasOwner() || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(entries_.size())) {
	  return;
   }

   auto& current = entries_[currentIndex_];
   if (current.objectName.empty()) {
	  return;
   }

   // エディタ操作中の移動・拡縮を CharacterLanding に毎フレーム反映する
   current.center = GetPlanetCenter(current.objectName);
   current.surfaceRadius = GetPlanetSurfaceRadius(current.objectName);

   if (auto* landing = GetOwner().GetComponent<CharacterLanding>()) {
	  landing->SetPlanetCenter(current.center);
	  landing->SetSurfaceRadius(current.surfaceRadius);
   }

   if (auto* bridge = GetOwner().GetComponent<CameraGravityBridge>()) {
	  bridge->SetPlanetCenter(current.center);
   }
}

bool PlanetSwitcher::HasPlanet(const std::string& objectName) const {
   return std::any_of(entries_.begin(), entries_.end(),
	  [&objectName](const PlanetEntry& entry) {
		 return entry.objectName == objectName;
	  });
}

nlohmann::json PlanetSwitcher::Serialize() const {
   nlohmann::json json;
   json["switchHysteresis"] = switchHysteresis;
   json["obbHalfExtents"] = SerializeVector3(obbHalfExtents);

   auto planets = nlohmann::json::array();
   for (const auto& entry : entries_) {
	  if (entry.objectName.empty()) {
		 continue;
	  }
	  planets.push_back({ { "objectName", entry.objectName } });
   }
   json["planets"] = planets;
   return json;
}

void PlanetSwitcher::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   if (data.contains("switchHysteresis") && data.at("switchHysteresis").is_number()) {
	  switchHysteresis = data.at("switchHysteresis").get<float>();
   }
   if (data.contains("obbHalfExtents")) {
	  obbHalfExtents = DeserializeVector3(data.at("obbHalfExtents"), obbHalfExtents);
   }

   entries_.clear();
   currentIndex_ = -1;

   if (!data.contains("planets") || !data.at("planets").is_array()) {
	  return;
   }

   for (const auto& planetData : data.at("planets")) {
	  std::string objectName;
	  if (planetData.is_string()) {
		 objectName = planetData.get<std::string>();
	  } else if (planetData.is_object() && planetData.contains("objectName") && planetData.at("objectName").is_string()) {
		 objectName = planetData.at("objectName").get<std::string>();
	  }

	  if (objectName.empty() || HasPlanet(objectName)) {
		 continue;
	  }

	  entries_.push_back({ objectName, GetPlanetCenter(objectName), GetPlanetSurfaceRadius(objectName) });
   }
}

#ifdef USE_IMGUI
void PlanetSwitcher::DrawInspector() {
   if (!ImGui::CollapsingHeader("PlanetSwitcher")) {
	  return;
   }
   ImGui::Separator();
   ImGui::Text("Planets: %d", static_cast<int>(entries_.size()));
   ImGui::Text("Current Planet Index: %d", currentIndex_);
   ImGui::DragFloat("Switch Hysteresis", &switchHysteresis, 0.05f, 0.0f, 20.0f);
   ImGui::DragFloat3("OBB Half Extents", &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);

   // 登録済み惑星の情報を表示/編集
   for (size_t i = 0; i < entries_.size(); ++i) {
	  auto& e = entries_[i];
	  ImGui::PushID(static_cast<int>(i));
	  ImGui::Text("Object Name: %s", e.objectName.c_str());
	  ImGui::Text("Center: (%.2f, %.2f, %.2f)", e.center.x, e.center.y, e.center.z);
	  ImGui::Text("Surface Radius: %.2f", e.surfaceRadius);
	  ImGui::PopID();
   }

   ImGui::SeparatorText("Add Planet");
   static char newObjectName[128] = "";
   ImGui::InputText("Object Name##PlanetSwitcherAddObjectName", newObjectName, sizeof(newObjectName));
   ImGui::SameLine();
   if (ImGui::Button("Add##PlanetSwitcherAddByName") && newObjectName[0] != '\0') {
	  AddPlanet(newObjectName);
	  newObjectName[0] = '\0';
   }

   ImGui::Button("Drop Hierarchy Object Here##PlanetSwitcherDropTarget", ImVec2(-1.0f, 0.0f));
   if (ImGui::BeginDragDropTarget()) {
	  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kSceneObjectPayload)) {
		 const char* objectName = static_cast<const char*>(payload->Data);
		 if (objectName && payload->DataSize > 1) {
			AddPlanet(objectName);
		 }
	  }
	  ImGui::EndDragDropTarget();
   }
}
#endif

GravityAttractor* PlanetSwitcher::GetAttractorByObjectName(const std::string& objectName) const {
   auto& models = GameEngine::Model::GetRegisteredModels();
   for (auto* model : models) {
	  if (model->GetObjectName() == objectName) {
		 auto* attractor = model->GetComponent<SphericalGravityAttractor>();
		 return attractor;
	  }
   }
   return nullptr;
}

GameEngine::Vector3 PlanetSwitcher::GetPlanetCenter(const std::string& objectName) const {
   auto& models = GameEngine::Model::GetRegisteredModels();
   for (auto* model : models) {
	  if (model->GetObjectName() == objectName) {
		 return model->GetPosition();
	  }
   }
   return { 0.0f, 0.0f, 0.0f };
}

float PlanetSwitcher::GetPlanetSurfaceRadius(const std::string& objectName) const
{
   auto& models = GameEngine::Model::GetRegisteredModels();
   for (auto* model : models) {
	  if (model->GetObjectName() == objectName) {
		 return model->GetScale().x; // 仮にスケールのX軸を半径として使用
	  }
   }
   return 0.0f;
}

} // namespace App
