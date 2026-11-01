#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include "GameEngine/Object/Model/Model.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameEngine/Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

namespace {

GameEngine::Model* FindPlanetById(const std::string& objectId) {
   return dynamic_cast<GameEngine::Model*>(GameEngine::Object::FindByEntityId(objectId));
}

}

/// @brief OBBと点の最短距離を返す
/// @param point  ワールド空間の点
/// @param obbCenter OBB中心（ワールド）
/// @param obbRot    OBBの向き（クォータニオン）
/// @param halfExtents OBBの半サイズ (scale * 0.5f)
static float DistancePointOBB(const GameEngine::Vector3& point, const GameEngine::Vector3& obbCenter,
   const GameEngine::Quaternion& obbRot, const GameEngine::Vector3& halfExtents) {
   // OBBローカル空間へ変換（逆回転）
   GameEngine::Quaternion invRot = obbRot.Inverse();
   GameEngine::Vector3 localVec = RotateVector(point - obbCenter, invRot);

   // 各軸でクランプして最近傍点を求め、距離を返す
   const GameEngine::Vector3 extents{std::abs(halfExtents.x), std::abs(halfExtents.y), std::abs(halfExtents.z)};
   float dx = localVec.x - std::clamp(localVec.x, -extents.x, extents.x);
   float dy = localVec.y - std::clamp(localVec.y, -extents.y, extents.y);
   float dz = localVec.z - std::clamp(localVec.z, -extents.z, extents.z);
   return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/// @brief OBBと惑星地表の最短距離を返す
static float DistancePlanetSurfaceToOBB(const GameEngine::Vector3& planetCenter, float planetRadius,
   const GameEngine::Vector3& obbCenter, const GameEngine::Quaternion& obbRot, const GameEngine::Vector3& halfExtents) {
   const float centerDistance = DistancePointOBB(planetCenter, obbCenter, obbRot, halfExtents);
   return std::max(centerDistance - std::max(planetRadius, 0.0f), 0.0f);
}

void PlanetSwitcher::Update(float) {
   if (!HasOwner()) return;
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) return;

   const auto& pose = transform->GetLocalPose();
   const auto selected = SelectBestPlanetIndex(pose.translation, pose.GetActiveQuaternion());
   selectedGravityIndex_ = selected;
   if (selected < 0) {
      currentIndex_ = -1;
      pendingIndex_ = -1;
      return;
   }

   // 選択結果だけを公開する。重力・接触・カメラは各フェーズで必要な結果を読む。
   if (IsOwnerAirborne()) {
      pendingIndex_ = selected;
   } else {
      currentIndex_ = selected;
      pendingIndex_ = -1;
   }
}

std::int32_t PlanetSwitcher::SelectBestPlanetIndex(
   const GameEngine::Vector3& pos, const GameEngine::Quaternion& obbRot) {
   const GameEngine::Vector3 halfExtents = obbHalfExtents;

   // 実際に重力を適用できる惑星の中から、OBBと地表の距離が最短のものを探索する。
   std::int32_t bestIndex = -1;
   float bestDistance = std::numeric_limits<float>::max();

   for (std::int32_t i = 0; i < static_cast<std::int32_t>(entries_.size()); ++i) {
      auto& e = entries_[i];

      if (e.objectId.empty()) {
         continue;
      }

      // 位置と半径を更新
      e.center = GetPlanetCenter(e.objectId);
      e.surfaceRadius = GetPlanetSurfaceRadius(e.objectId);

      auto* attractor = GetAttractorByObjectId(e.objectId);

      if (!attractor || !attractor->IsEnabled() || !attractor->IsInRange(pos)) {
         continue;
      }

      const float distance = DistancePlanetSurfaceToOBB(e.center, e.surfaceRadius, pos, obbRot, halfExtents);

      if (distance < bestDistance) {
         bestDistance = distance;
         bestIndex = i;
      }
   }

   if (bestIndex < 0) {
      return -1;
   }

   std::int32_t newIndex = bestIndex;

   // 空中では着地済み惑星ではなく、現在実際に接続中の重力先を基準にする。
   const std::int32_t referenceIndex = selectedGravityIndex_ >= 0 ? selectedGravityIndex_ : currentIndex_;

   if (newIndex != referenceIndex && referenceIndex >= 0 &&
       referenceIndex < static_cast<std::int32_t>(entries_.size())) {
      const auto& reference = entries_[referenceIndex];
      auto* referenceAttractor = GetAttractorByObjectId(reference.objectId);

      // 圏外・無効な旧惑星をヒステリシスで維持すると重力が適用されないため、保持対象から外す。
      if (referenceAttractor && referenceAttractor->IsEnabled() && referenceAttractor->IsInRange(pos)) {
         const float referenceDistance =
            DistancePlanetSurfaceToOBB(reference.center, reference.surfaceRadius, pos, obbRot, halfExtents);

         if (referenceDistance - bestDistance < std::max(switchHysteresis, 0.0f)) {
            newIndex = referenceIndex;
         }
      }
   }

   return newIndex;
}

void PlanetSwitcher::AddPlanet(std::string objectId) {
   if (objectId.empty() || HasPlanet(objectId)) return;

   if (auto* model = FindPlanetById(objectId)) {
      objectId = model->GetEntityId();

      if (HasPlanet(objectId)) return;

      GameEngine::Vector3 modelPos = model->GetPosition();
      float surfaceRadius = GetPlanetSurfaceRadius(objectId);
      entries_.push_back({objectId, modelPos, surfaceRadius});
   }
}

void PlanetSwitcher::OnReferencesChanged(GameEngine::SceneWorld&) {
   for (auto& entry : entries_) {
      if (auto* model = FindPlanetById(entry.objectId)) {
         entry.center = GetPlanetCenter(entry.objectId);
         entry.surfaceRadius = GetPlanetSurfaceRadius(entry.objectId);
      }
   }
}

bool PlanetSwitcher::RemovePlanet(size_t index) {
   if (index >= entries_.size()) {
      return false;
   }

   entries_.erase(entries_.begin() + index);
   const auto remap = [index](std::int32_t& selected) {
      if (selected == static_cast<std::int32_t>(index)) {
         selected = -1;
      } else if (selected > static_cast<std::int32_t>(index)) {
         --selected;
      }
   };
   remap(currentIndex_);
   remap(pendingIndex_);
   remap(selectedGravityIndex_);

   return true;
}

bool PlanetSwitcher::MovePlanet(size_t from, size_t to) {
   if (from >= entries_.size() || to >= entries_.size() || from == to) {
      return false;
   }

   std::swap(entries_[from], entries_[to]);
   const auto remap = [from, to](std::int32_t& selected) {
      if (selected == static_cast<std::int32_t>(from)) {
         selected = static_cast<std::int32_t>(to);
      } else if (selected == static_cast<std::int32_t>(to)) {
         selected = static_cast<std::int32_t>(from);
      }
   };
   remap(currentIndex_);
   remap(pendingIndex_);
   remap(selectedGravityIndex_);
   return true;
}

bool PlanetSwitcher::TryGetLandingPlanet(GameEngine::Vector3& outCenter, float& outSurfaceRadius) const {
   // 空中では保留候補を返し、カメラの着地予測だけを実際の重力先へ先行させる。
   std::int32_t index = pendingIndex_ >= 0 ? pendingIndex_ : currentIndex_;

   if (!IsEnabled() || index < 0 || index >= static_cast<std::int32_t>(entries_.size())) {
      return false;
   }

   const PlanetEntry& entry = entries_[index];
   outCenter = entry.center;
   outSurfaceRadius = entry.surfaceRadius;
   return true;
}

void PlanetSwitcher::CommitPendingSwitch() {
   if (pendingIndex_ < 0) return;

   // 接触した候補の確定だけを行い、力の再適用や他部品の設定変更は行わない。
   currentIndex_ = pendingIndex_;
   pendingIndex_ = -1;
}

GravityAttractor* PlanetSwitcher::GetSelectedAttractor() const {
   if (!IsEnabled() || selectedGravityIndex_ < 0 ||
       selectedGravityIndex_ >= static_cast<std::int32_t>(entries_.size())) {
      return nullptr;
   }
   return GetAttractorByObjectId(entries_[selectedGravityIndex_].objectId);
}

bool PlanetSwitcher::TryGetCurrentPlanet(GameEngine::Vector3& center, float& radius) const {
   if (!IsEnabled() || currentIndex_ < 0 || currentIndex_ >= static_cast<std::int32_t>(entries_.size())) {
      return false;
   }
   const auto& entry = entries_[currentIndex_];
   center = GetPlanetCenter(entry.objectId);
   radius = GetPlanetSurfaceRadius(entry.objectId);
   return radius > 0.0f;
}

bool PlanetSwitcher::IsOwnerAirborne() const {
   if (!HasOwner()) {
      return false;
   }

   auto* jump = GetOwner().GetComponent<CharacterJump>();
   return jump && jump->IsJumping();
}

bool PlanetSwitcher::HasPlanet(const std::string& objectId) const {
   return std::any_of(
      entries_.begin(), entries_.end(), [&objectId](const PlanetEntry& entry) { return entry.objectId == objectId; });
}

nlohmann::json PlanetSwitcher::Serialize() const {
   nlohmann::json json;
   json["switchHysteresis"] = switchHysteresis;
   json["obbHalfExtents"] = GameEngine::JsonValue::WriteVector3(obbHalfExtents);

   auto planets = nlohmann::json::array();

   for (const auto& entry : entries_) {
      if (entry.objectId.empty()) {
         continue;
      }

      planets.push_back({{"objectId", entry.objectId}});
   }

   json["planets"] = planets;
   return json;
}

nlohmann::json PlanetSwitcher::GetSharedParameters() const {
   return ExtractSharedParameters({"planets"});
}

void PlanetSwitcher::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void PlanetSwitcher::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void PlanetSwitcher::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   if (data.contains("switchHysteresis") && data.at("switchHysteresis").is_number()) {
      switchHysteresis = std::max(data.at("switchHysteresis").get<float>(), 0.0f);
   }

   if (data.contains("obbHalfExtents")) {
      obbHalfExtents = GameEngine::JsonValue::ReadVector3(data.at("obbHalfExtents"), obbHalfExtents);
   }

   if (initializeRuntime) {
      entries_.clear();

      // 読み直した配列と旧インデックスの対応は保証できないため、選択状態も同時に破棄する。
      currentIndex_ = -1;
      pendingIndex_ = -1;
      selectedGravityIndex_ = -1;

      if (!data.contains("planets") || !data.at("planets").is_array()) return;

      for (const auto& planetData : data.at("planets")) {
         std::string objectId;

         if (planetData.is_object() && planetData.contains("objectId") && planetData.at("objectId").is_string()) {
            objectId = planetData.at("objectId").get<std::string>();
         }

         if (objectId.empty() || HasPlanet(objectId)) {
            continue;
         }

         entries_.push_back({objectId, GetPlanetCenter(objectId), GetPlanetSurfaceRadius(objectId)});
      }
   }

   Configure(DescribeSettings());
}

#ifdef USE_IMGUI
void PlanetSwitcher::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::Text("%s: %d", Tr("惑星数", "Planets"), static_cast<std::int32_t>(entries_.size()));
   ImGui::Text("%s: %d", Tr("現在の惑星インデックス", "Current Planet Index"), currentIndex_);
   ImGui::DragFloat(Tr("切り替えヒステリシス", "Switch Hysteresis"), &switchHysteresis, 0.05f, 0.0f, 20.0f);
   ImGui::DragFloat3(Tr("OBB半径", "OBB Half Extents"), &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);

   for (size_t i = 0; i < entries_.size(); ++i) {
      ImGui::PushID(static_cast<std::int32_t>(i));
      const std::string previousId = entries_[i].objectId;

      if (GameEngine::EditorUI::ObjectReference("Planet", entries_[i].objectId, "SphericalGravityAttractor")) {
         if (entries_[i].objectId.empty()) {
            ImGui::PopID();
            RemovePlanet(i);
            break;
         }

         for (size_t other = 0; other < entries_.size(); ++other) {
            if (other != i && entries_[other].objectId == entries_[i].objectId) {
               entries_[i].objectId = previousId;
               break;
            }
         }
      }

      ImGui::BeginDisabled(i == 0);
      const bool up = ImGui::SmallButton("Up");
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(i + 1 == entries_.size());
      const bool down = ImGui::SmallButton("Down");
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool remove = ImGui::SmallButton("Remove");
      ImGui::PopID();

      if (remove) {
         RemovePlanet(i);
         break;
      }

      if (up || down) {
         MovePlanet(i, up ? i - 1 : i + 1);
         break;
      }
   }

   GameEngine::EditorUI::ObjectReference("Add Planet", newPlanetId_, "SphericalGravityAttractor");

   if (ImGui::Button(Tr("追加", "Add")) && !newPlanetId_.empty()) {
      AddPlanet(newPlanetId_);
      newPlanetId_.clear();
   }

   Configure(DescribeSettings());
}
#endif

GravityAttractor* PlanetSwitcher::GetAttractorByObjectId(const std::string& objectId) const {
   if (auto* model = FindPlanetById(objectId)) {
      return model->GetComponent<SphericalGravityAttractor>();
   }

   return nullptr;
}

GameEngine::Vector3 PlanetSwitcher::GetPlanetCenter(const std::string& objectId) const {
   if (auto* model = FindPlanetById(objectId)) {
      return model->GetPosition();
   }

   return {0.0f, 0.0f, 0.0f};
}

float PlanetSwitcher::GetPlanetSurfaceRadius(const std::string& objectId) const {
   if (auto* model = FindPlanetById(objectId)) {
      const auto scale = model->GetScale();

      // 非一様・負スケールでも地表距離が負や過小にならないよう最大軸を球半径に使う。
      return std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)}) * 0.5f;
   }

   return 0.0f;
}

bool PlanetSwitcher::TryGetLandingPlanet(
   GameEngine::Model*& outReceiver, GameEngine::Vector3& outCenter, float& outSurfaceRadius) const {
   // 失敗時に呼び出し側へ古い値を残さない。
   outReceiver = nullptr;
   outCenter = {};
   outSurfaceRadius = 0.0f;

   // 空中で次の着地候補が決まっている場合は、その惑星を優先する。
   const std::int32_t index = pendingIndex_ >= 0 ? pendingIndex_ : currentIndex_;

   if (!IsEnabled() || index < 0 || index >= static_cast<std::int32_t>(entries_.size())) {
      return false;
   }

   const auto& entry = entries_[index];

   auto* receiver = FindPlanetById(entry.objectId);

   if (!receiver) {
      return false;
   }

   // 既存の着地判定と同じ規約で、
   // 選択した惑星の中心・半径を取得する。
   const auto center = GetPlanetCenter(entry.objectId);
   const float radius = GetPlanetSurfaceRadius(entry.objectId);

   if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z) || !std::isfinite(radius) ||
       radius <= 0.0f) {
      return false;
   }

   outReceiver = receiver;
   outCenter = center;
   outSurfaceRadius = radius;
   return true;
}

} // namespace App

namespace App {

PlanetSwitcher::Settings PlanetSwitcher::DescribeSettings() const {
   Settings settings;
   settings.switchHysteresis = switchHysteresis;
   settings.obbHalfExtents = obbHalfExtents;
   return settings;
}

void PlanetSwitcher::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.switchHysteresis)) {
      settings.switchHysteresis = defaults.switchHysteresis;
   }

   if (!std::isfinite(settings.obbHalfExtents.x)) {
      settings.obbHalfExtents.x = defaults.obbHalfExtents.x;
   }

   if (!std::isfinite(settings.obbHalfExtents.y)) {
      settings.obbHalfExtents.y = defaults.obbHalfExtents.y;
   }

   if (!std::isfinite(settings.obbHalfExtents.z)) {
      settings.obbHalfExtents.z = defaults.obbHalfExtents.z;
   }

   switchHysteresis = settings.switchHysteresis;
   obbHalfExtents = settings.obbHalfExtents;
}

} // namespace App
