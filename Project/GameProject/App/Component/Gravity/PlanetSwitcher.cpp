#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Gravity/GravityAttractorLink.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterWalker.h"
#include "GameProject/App/Component/Camera/CameraGravityBridge.h"
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
   // オーナー不在または候補未登録なら何もしない
   if (!HasOwner() || entries_.empty()) return;

   // 現在位置を取得
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();

   if (!transform) return;

   const GameEngine::Vector3 pos = transform->GetLocalPose().translation;

   const GameEngine::Quaternion obbRot = transform->GetLocalPose().GetActiveQuaternion();
   std::int32_t newIndex = SelectBestPlanetIndex(pos, obbRot);

   if (newIndex < 0) {
      // 候補が全て削除・無効・圏外の場合、古い惑星の重力を持ち越さない。
      pendingIndex_ = -1;
      activeGravityIndex_ = -1;

      if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
         link->SetAttractor(nullptr);
      }

      if (auto* gravityBody = GetOwner().GetComponent<GravityBody>()) {
         gravityBody->ReleaseGravity();
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
   const std::int32_t referenceIndex = activeGravityIndex_ >= 0 ? activeGravityIndex_ : currentIndex_;

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

void PlanetSwitcher::ApplyPlanetIndex(std::int32_t newIndex) {
   if (newIndex < 0 || newIndex >= static_cast<std::int32_t>(entries_.size())) return;

   const bool planetChanged = (newIndex != currentIndex_);
   const bool gravityChanged = (newIndex != activeGravityIndex_);

   // 着地基準の惑星と、空中ですでに切り替えた重力先は別インデックスで追跡する。
   if (planetChanged) {
      currentIndex_ = newIndex;
   }

   const auto& best = entries_[newIndex];
   auto* attractor = GetAttractorByObjectId(best.objectId);

   // 毎フレーム参照を更新し、エディタでの削除・再生成や有効状態変更にも追従する。
   if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
      link->SetAttractor(attractor);
   }

   bool gravityApplied = false;
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();

   if (attractor && gravityBody && transform) {
      gravityApplied = attractor->ApplyTo(*gravityBody, transform->GetLocalPose().translation);
   }

   if (!gravityApplied && gravityBody) {
      gravityBody->ReleaseGravity();
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

void PlanetSwitcher::ApplyAirborneAttractorIndex(std::int32_t newIndex, const GameEngine::Vector3& pos) {
   if (newIndex < 0 || newIndex >= static_cast<std::int32_t>(entries_.size())) return;

   const auto& candidate = entries_[newIndex];

   if (candidate.objectId.empty()) return;

   auto* attractor = GetAttractorByObjectId(candidate.objectId);

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
         gravityBody->ReleaseGravity();
      }
   }
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
   remap(activeGravityIndex_);

   if (activeGravityIndex_ < 0 && HasOwner()) {
      if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
         link->SetAttractor(nullptr);
      }

      if (auto* body = GetOwner().GetComponent<GravityBody>()) {
         body->ReleaseGravity();
      }
   }

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
   remap(activeGravityIndex_);
   return true;
}

bool PlanetSwitcher::TryGetLandingPlanet(GameEngine::Vector3& outCenter, float& outSurfaceRadius) const {
   // 空中では保留候補を返し、カメラの着地予測だけを実際の重力先へ先行させる。
   std::int32_t index = pendingIndex_ >= 0 ? pendingIndex_ : currentIndex_;

   if (index < 0 || index >= static_cast<std::int32_t>(entries_.size())) {
      return false;
   }

   const PlanetEntry& entry = entries_[index];
   outCenter = entry.center;
   outSurfaceRadius = entry.surfaceRadius;
   return true;
}

void PlanetSwitcher::CommitPendingSwitch() {
   if (pendingIndex_ < 0) return;

   std::int32_t index = pendingIndex_;

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
   if (!HasOwner() || currentIndex_ < 0 || currentIndex_ >= static_cast<std::int32_t>(entries_.size())) return;

   auto& current = entries_[currentIndex_];

   if (current.objectId.empty()) return;

   // エディタ操作中の移動・拡縮を CharacterLanding に毎フレーム反映する
   current.center = GetPlanetCenter(current.objectId);
   current.surfaceRadius = GetPlanetSurfaceRadius(current.objectId);

   if (auto* landing = GetOwner().GetComponent<CharacterLanding>()) {
      landing->SetPlanetCenter(current.center);
      landing->SetSurfaceRadius(current.surfaceRadius);
   }

   if (auto* bridge = GetOwner().GetComponent<CameraGravityBridge>()) {
      bridge->SetPlanetCenter(current.center);
   }
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
   activeGravityIndex_ = -1;

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

   if (index < 0 || index >= static_cast<std::int32_t>(entries_.size())) {
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
