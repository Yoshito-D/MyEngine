#include "PlanetSwitcher.h"
#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterJump.h"
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
   const GameEngine::Vector3 extents{
      std::abs(halfExtents.x),
      std::abs(halfExtents.y),
      std::abs(halfExtents.z)
   };
   float dx = localVec.x - std::clamp(localVec.x, -extents.x, extents.x);
   float dy = localVec.y - std::clamp(localVec.y, -extents.y, extents.y);
   float dz = localVec.z - std::clamp(localVec.z, -extents.z, extents.z);
   return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/// @brief OBBと惑星地表の最短距離を返す
static float DistancePlanetSurfaceToOBB(
   const GameEngine::Vector3& planetCenter,
   float planetRadius,
   const GameEngine::Vector3& obbCenter,
   const GameEngine::Quaternion& obbRot,
   const GameEngine::Vector3& halfExtents)
{
   const float centerDistance = DistancePointOBB(planetCenter, obbCenter, obbRot, halfExtents);
   return std::max(centerDistance - std::max(planetRadius, 0.0f), 0.0f);
}

void PlanetSwitcher::Update(float) {
   // オーナー不在または候補未登録なら何もしない
   if (!HasOwner() || entries_.empty()) { return; }

   // 現在位置を取得
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }
   const GameEngine::Vector3 pos = transform->transform.translation;

   const GameEngine::Quaternion obbRot = transform->transform.GetActiveQuaternion();
   int newIndex = SelectBestPlanetIndex(pos, obbRot);
   if (newIndex < 0) {
	  // 候補が全て削除・無効・圏外の場合、古い惑星の重力を持ち越さない。
	  pendingIndex_ = -1;
	  activeGravityIndex_ = -1;
	  if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
		 link->SetAttractor(nullptr);
	  }
	  if (auto* gravityBody = GetOwner().GetComponent<GravityBody>()) {
		 gravityBody->SetGravity({ 0.0f, 0.0f, 0.0f });
	  }
	  return;
   }

   if (IsOwnerAirborne()) {
	  // 空中ではカメラなどの基準確定は保留しつつ、重力だけ近い惑星へ向ける。
	  pendingIndex_ = newIndex;
	  ApplyAirborneAttractorIndex(newIndex, pos);
	  return;
   }

   pendingIndex_ = -1;
   ApplyPlanetIndex(newIndex);
}

int PlanetSwitcher::SelectBestPlanetIndex(const GameEngine::Vector3& pos,
										  const GameEngine::Quaternion& obbRot) {
   const GameEngine::Vector3 halfExtents = obbHalfExtents;

   // 実際に重力を適用できる惑星の中から、OBBと地表の距離が最短のものを探索する。
   int bestIndex = -1;
   float bestDistance = std::numeric_limits<float>::max();

   for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
	  auto& e = entries_[i];
	  if (e.objectName.empty()) { continue; }

	  // 位置と半径を更新
	  e.center = GetPlanetCenter(e.objectName);
	  e.surfaceRadius = GetPlanetSurfaceRadius(e.objectName);

	  auto* attractor = GetAttractorByObjectName(e.objectName);
	  if (!attractor || !attractor->IsEnabled() || !attractor->IsInRange(pos)) {
		 continue;
	  }

	  const float distance = DistancePlanetSurfaceToOBB(
		 e.center, e.surfaceRadius, pos, obbRot, halfExtents);
	  if (distance < bestDistance) {
		 bestDistance = distance;
		 bestIndex = i;
	  }
   }

   if (bestIndex < 0) {
	  return -1;
   }

   int newIndex = bestIndex;

   // 空中では着地済み惑星ではなく、現在実際に接続中の重力先を基準にする。
   const int referenceIndex = activeGravityIndex_ >= 0 ? activeGravityIndex_ : currentIndex_;
   if (newIndex != referenceIndex &&
	  referenceIndex >= 0 && referenceIndex < static_cast<int>(entries_.size())) {
	  const auto& reference = entries_[referenceIndex];
	  auto* referenceAttractor = GetAttractorByObjectName(reference.objectName);
	  // 圏外・無効な旧惑星をヒステリシスで維持すると重力が適用されないため、保持対象から外す。
	  if (referenceAttractor && referenceAttractor->IsEnabled() && referenceAttractor->IsInRange(pos)) {
		 const float referenceDistance = DistancePlanetSurfaceToOBB(
			reference.center, reference.surfaceRadius, pos, obbRot, halfExtents);
		 if (referenceDistance - bestDistance < std::max(switchHysteresis, 0.0f)) {
			newIndex = referenceIndex;
		 }
	  }
   }

   return newIndex;
}

void PlanetSwitcher::ApplyPlanetIndex(int newIndex) {
   if (newIndex < 0 || newIndex >= static_cast<int>(entries_.size())) {
	  return;
   }

   const bool planetChanged = (newIndex != currentIndex_);
   const bool gravityChanged = (newIndex != activeGravityIndex_);
   // 着地基準の惑星と、空中ですでに切り替えた重力先は別インデックスで追跡する。
   if (planetChanged) {
	  currentIndex_ = newIndex;
   }

   const auto& best = entries_[newIndex];
   auto* attractor = GetAttractorByObjectName(best.objectName);

   // 毎フレーム参照を更新し、エディタでの削除・再生成や有効状態変更にも追従する。
   if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
	  link->SetAttractor(attractor);
   }

   bool gravityApplied = false;
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (attractor && gravityBody && transform) {
	  gravityApplied = attractor->ApplyTo(*gravityBody, transform->transform.translation);
   }
   if (!gravityApplied && gravityBody) {
	  gravityBody->SetGravity({ 0.0f, 0.0f, 0.0f });
   }

   if (gravityApplied) {
	  activeGravityIndex_ = newIndex;
	  if (gravityChanged) {
		 switched_ = true;
	  }
   } else {
	  activeGravityIndex_ = -1;
   }

   if (planetChanged) {
	  if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
		 walker->ResetHorizontalVelocity();
	  }
   }

   ApplyCurrentPlanetParameters();
}

void PlanetSwitcher::ApplyAirborneAttractorIndex(int newIndex, const GameEngine::Vector3& pos) {
   if (newIndex < 0 || newIndex >= static_cast<int>(entries_.size())) {
	  return;
   }

   const auto& candidate = entries_[newIndex];
   if (candidate.objectName.empty()) {
	  return;
   }

   auto* attractor = GetAttractorByObjectName(candidate.objectName);
   if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
	  link->SetAttractor(attractor);
   }

   // GravityAttractorLink はこのフレームでは既に更新済みなので、
   // 空中切替直後の物理積分にも新しい重力を反映する。
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   const bool gravityChanged = activeGravityIndex_ != newIndex;
   if (attractor && gravityBody && attractor->ApplyTo(*gravityBody, pos)) {
	  activeGravityIndex_ = newIndex;
	  if (gravityChanged) {
		 switched_ = true;
	  }
   } else {
	  activeGravityIndex_ = -1;
	  if (gravityBody) {
		 gravityBody->SetGravity({ 0.0f, 0.0f, 0.0f });
	  }
   }
}

void PlanetSwitcher::AddPlanet(std::string objectName) {
   if (objectName.empty() || HasPlanet(objectName)) {
	  return;
   }

   if (auto* model = FindRegisteredModelByObjectName(objectName)) {
	  GameEngine::Vector3 modelPos = model->GetPosition();
	  float surfaceRadius = GetPlanetSurfaceRadius(objectName);
	  entries_.push_back({ objectName, modelPos, surfaceRadius });
   }
}

bool PlanetSwitcher::TryGetLandingPlanet(GameEngine::Vector3& outCenter, float& outSurfaceRadius) const {
   // 空中では保留候補を返し、カメラの着地予測だけを実際の重力先へ先行させる。
   int index = pendingIndex_ >= 0 ? pendingIndex_ : currentIndex_;
   if (index < 0 || index >= static_cast<int>(entries_.size())) {
	  return false;
   }

   const PlanetEntry& entry = entries_[index];
   outCenter = entry.center;
   outSurfaceRadius = entry.surfaceRadius;
   return true;
}

void PlanetSwitcher::CommitPendingSwitch() {
   if (pendingIndex_ < 0) {
	  return;
   }

   int index = pendingIndex_;
   // 接地処理から呼ばれた時点で、保留していた惑星を正式な地表・カメラ基準へ昇格する。
   pendingIndex_ = -1;
   ApplyPlanetIndex(index);
}

bool PlanetSwitcher::IsOwnerAirborne() const {
   if (!HasOwner()) {
	  return false;
   }

   auto* jump = GetOwner().GetComponent<CharacterJump>();
   return jump && jump->IsJumping();
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
	  switchHysteresis = std::max(data.at("switchHysteresis").get<float>(), 0.0f);
   }
   if (data.contains("obbHalfExtents")) {
	  obbHalfExtents = DeserializeVector3(data.at("obbHalfExtents"), obbHalfExtents);
   }

   entries_.clear();
   // 読み直した配列と旧インデックスの対応は保証できないため、選択状態も同時に破棄する。
   currentIndex_ = -1;
   pendingIndex_ = -1;
   activeGravityIndex_ = -1;

   if (!data.contains("planets") || !data.at("planets").is_array()) {
	  return;
   }

   for (const auto& planetData : data.at("planets")) {
	  std::string objectName;
	  // 旧形式の文字列配列と現行のobjectNameオブジェクトをどちらも受け付ける。
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
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
	  return;
   }
   ImGui::Separator();
   ImGui::Text("%s: %d", Tr("惑星数", "Planets"), static_cast<int>(entries_.size()));
   ImGui::Text("%s: %d", Tr("現在の惑星インデックス", "Current Planet Index"), currentIndex_);
   ImGui::DragFloat(Tr("切り替えヒステリシス", "Switch Hysteresis"), &switchHysteresis, 0.05f, 0.0f, 20.0f);
   ImGui::DragFloat3(Tr("OBB半径", "OBB Half Extents"), &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);

   // 登録済み惑星の情報を表示/編集
   for (size_t i = 0; i < entries_.size(); ++i) {
	  auto& e = entries_[i];
	  ImGui::PushID(static_cast<int>(i));
	  ImGui::Text("%s: %s", Tr("オブジェクト名", "Object Name"), e.objectName.c_str());
	  ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("中心", "Center"), e.center.x, e.center.y, e.center.z);
	  ImGui::Text("%s: %.2f", Tr("地表半径", "Surface Radius"), e.surfaceRadius);
	  ImGui::PopID();
   }

   ImGui::SeparatorText(Tr("惑星を追加", "Add Planet"));
   static char newObjectName[128] = "";
   ImGui::InputText((std::string(Tr("オブジェクト名", "Object Name")) + "##PlanetSwitcherAddObjectName").c_str(), newObjectName, sizeof(newObjectName));
   ImGui::SameLine();
   if (ImGui::Button((std::string(Tr("追加", "Add")) + "##PlanetSwitcherAddByName").c_str()) && newObjectName[0] != '\0') {
	  AddPlanet(newObjectName);
	  newObjectName[0] = '\0';
   }

   ImGui::Button((std::string(Tr("ヒエラルキーのオブジェクトをここへドロップ", "Drop Hierarchy Object Here")) + "##PlanetSwitcherDropTarget").c_str(), ImVec2(-1.0f, 0.0f));
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
   if (auto* model = FindRegisteredModelByObjectName(objectName)) {
	  return model->GetComponent<SphericalGravityAttractor>();
   }
   return nullptr;
}

GameEngine::Vector3 PlanetSwitcher::GetPlanetCenter(const std::string& objectName) const {
   if (auto* model = FindRegisteredModelByObjectName(objectName)) {
	  return model->GetPosition();
   }
   return { 0.0f, 0.0f, 0.0f };
}

float PlanetSwitcher::GetPlanetSurfaceRadius(const std::string& objectName) const
{
   if (auto* model = FindRegisteredModelByObjectName(objectName)) {
	  const auto scale = model->GetScale();
	  // 非一様・負スケールでも地表距離が負や過小にならないよう最大軸を球半径に使う。
	  return std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) }) * 0.5f;
   }
   return 0.0f;
}

} // namespace App
