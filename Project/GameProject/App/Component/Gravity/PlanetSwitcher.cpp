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
#include <utility>
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

namespace {

bool TryReadPlanetSurface(const GameEngine::Model& planet, GameEngine::Vector3& center, float& radius) {
   const auto* transform = planet.GetComponent<GameEngine::TransformComponent>();

   if (!transform) {
      return false;
   }

   const auto& pose = transform->GetLocalPose();
   const auto& scale = pose.scale;

   // 非一様・負スケールでも地表距離が負や過小にならないよう最大軸を球半径に使う。
   constexpr float kUnscaledPlanetRadius = 0.5f; ///< 拡縮前の惑星メッシュの半径（m）。
   const float surfaceRadius =
      std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)}) * kUnscaledPlanetRadius;

   if (!pose.IsFinite() || !std::isfinite(surfaceRadius) || surfaceRadius <= 0.f) {
      return false;
   }

   center = pose.translation;
   radius = surfaceRadius;
   return true;
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
   const GameEngine::Vector3& pos, const GameEngine::Quaternion& obbRot) const {
   const GameEngine::Vector3 halfExtents = obbHalfExtents;

   // 空中では着地済み惑星ではなく、現在実際に接続中の重力先を基準にする。
   const std::int32_t referenceIndex = selectedGravityIndex_ >= 0 ? selectedGravityIndex_ : currentIndex_;
   float referenceDistance = std::numeric_limits<float>::infinity();

   // 実際に重力を適用できる惑星の中から、OBBと地表の距離が最短のものを探索する。
   std::int32_t bestIndex = -1;
   float bestDistance = std::numeric_limits<float>::max();

   for (std::int32_t i = 0; i < GetPlanetCount(); ++i) {
      const auto* planet = GetPlanetModel(i);
      const auto* attractor = planet ? planet->GetComponent<SphericalGravityAttractor>() : nullptr;

      if (!attractor || !attractor->IsEnabled() || !attractor->IsInRange(pos)) {
         continue;
      }

      GameEngine::Vector3 center{};
      float radius = 0.f;

      if (!TryReadPlanetSurface(*planet, center, radius)) {
         continue;
      }

      const float distance = DistancePlanetSurfaceToOBB(center, radius, pos, obbRot, halfExtents);

      if (distance < bestDistance) {
         bestDistance = distance;
         bestIndex = i;
      }

      // 圏外・無効な旧惑星をヒステリシスで維持すると重力が適用されないため、保持対象から外す。
      if (i == referenceIndex) {
         referenceDistance = distance;
      }
   }

   if (bestIndex >= 0 && referenceDistance - bestDistance < std::max(switchHysteresis, 0.0f)) {
      return referenceIndex;
   }

   return bestIndex;
}

void PlanetSwitcher::AddPlanet(std::string objectId) {
   if (objectId.empty() || HasPlanet(objectId)) return;

   if (FindPlanetById(objectId)) {
      planetIds_.push_back(std::move(objectId));
   }
}

bool PlanetSwitcher::RemovePlanet(size_t index) {
   if (index >= planetIds_.size()) {
      return false;
   }

   planetIds_.erase(planetIds_.begin() + index);
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
   if (from >= planetIds_.size() || to >= planetIds_.size() || from == to) {
      return false;
   }

   std::swap(planetIds_[from], planetIds_[to]);
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
   const auto* planet = IsEnabled() ? GetPlanetModel(GetLandingPlanetIndex()) : nullptr;
   return planet && TryReadPlanetSurface(*planet, outCenter, outSurfaceRadius);
}

std::int32_t PlanetSwitcher::GetLandingPlanetIndex() const {
   // 空中では保留候補を返し、カメラの着地予測だけを実際の重力先へ先行させる。
   return pendingIndex_ >= 0 ? pendingIndex_ : currentIndex_;
}

void PlanetSwitcher::CommitPendingSwitch() {
   if (pendingIndex_ < 0) return;

   // 接触した候補の確定だけを行い、力の再適用や他部品の設定変更は行わない。
   currentIndex_ = pendingIndex_;
   pendingIndex_ = -1;
}

GravityAttractor* PlanetSwitcher::GetSelectedAttractor() const {
   auto* planet = IsEnabled() ? GetPlanetModel(selectedGravityIndex_) : nullptr;
   return planet ? planet->GetComponent<SphericalGravityAttractor>() : nullptr;
}

GameEngine::Model* PlanetSwitcher::FindPlanetById(const std::string& objectId) const {
   auto* world = HasOwner() ? GetOwner().GetWorld() : nullptr;
   return world ? dynamic_cast<GameEngine::Model*>(world->FindObjectById(objectId)) : nullptr;
}

GameEngine::Model* PlanetSwitcher::GetPlanetModel(std::int32_t index) const {
   if (index < 0 || index >= GetPlanetCount()) {
      return nullptr;
   }

   return FindPlanetById(planetIds_[index]);
}

bool PlanetSwitcher::TryGetCurrentPlanet(GameEngine::Vector3& center, float& radius) const {
   const auto* planet = IsEnabled() ? GetPlanetModel(currentIndex_) : nullptr;
   return planet && TryReadPlanetSurface(*planet, center, radius);
}

bool PlanetSwitcher::TryGetCurrentPlanet(GameEngine::Model*& model, GameEngine::Vector3& center, float& radius) const {
   model = nullptr;
   center = {};
   radius = 0.f;

   auto* planet = IsEnabled() ? GetPlanetModel(currentIndex_) : nullptr;

   if (!planet || !TryReadPlanetSurface(*planet, center, radius)) {
      return false;
   }

   model = planet;
   return true;
}

bool PlanetSwitcher::IsOwnerAirborne() const {
   if (!HasOwner()) {
      return false;
   }

   auto* jump = GetOwner().GetComponent<CharacterJump>();
   return jump && jump->IsJumping();
}

bool PlanetSwitcher::HasPlanet(const std::string& objectId) const {
   return std::find(planetIds_.begin(), planetIds_.end(), objectId) != planetIds_.end();
}

nlohmann::json PlanetSwitcher::Serialize() const {
   nlohmann::json json;
   json["switchHysteresis"] = switchHysteresis;
   json["obbHalfExtents"] = GameEngine::JsonValue::WriteVector3(obbHalfExtents);

   auto planets = nlohmann::json::array();

   for (const auto& id : planetIds_) {
      planets.push_back({{"objectId", id}});
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
      obbHalfExtents = GameEngine::JsonValue::ReadVector3(data.at("obbHalfExtents"));
   }

   if (initializeRuntime) {
      planetIds_.clear();

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

         planetIds_.push_back(std::move(objectId));
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
   ImGui::Text("%s: %d", Tr("惑星数", "Planets"), GetPlanetCount());
   ImGui::Text("%s: %d", Tr("現在の惑星インデックス", "Current Planet Index"), currentIndex_);
   ImGui::DragFloat(Tr("切り替えヒステリシス", "Switch Hysteresis"), &switchHysteresis, 0.05f, 0.0f, 20.0f);
   ImGui::DragFloat3(Tr("OBB半径", "OBB Half Extents"), &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);

   for (size_t i = 0; i < planetIds_.size(); ++i) {
      ImGui::PushID(static_cast<std::int32_t>(i));
      const std::string previousId = planetIds_[i];

      if (GameEngine::EditorUI::ObjectReference("Planet", planetIds_[i], "SphericalGravityAttractor")) {
         if (planetIds_[i].empty()) {
            ImGui::PopID();
            RemovePlanet(i);
            break;
         }

         for (size_t other = 0; other < planetIds_.size(); ++other) {
            if (other != i && planetIds_[other] == planetIds_[i]) {
               planetIds_[i] = previousId;
               break;
            }
         }
      }

      ImGui::BeginDisabled(i == 0);
      const bool up = ImGui::SmallButton("Up");
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(i + 1 == planetIds_.size());
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

bool PlanetSwitcher::TryGetLandingPlanet(
   GameEngine::Model*& outReceiver, GameEngine::Vector3& outCenter, float& outSurfaceRadius) const {
   // 失敗時に呼び出し側へ古い値を残さない。
   outReceiver = nullptr;
   outCenter = {};
   outSurfaceRadius = 0.0f;

   auto* planet = IsEnabled() ? GetPlanetModel(GetLandingPlanetIndex()) : nullptr;

   if (!planet || !TryReadPlanetSurface(*planet, outCenter, outSurfaceRadius)) {
      return false;
   }

   outReceiver = planet;
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
