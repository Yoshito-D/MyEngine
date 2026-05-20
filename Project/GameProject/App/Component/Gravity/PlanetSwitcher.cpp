#include "PlanetSwitcher.h"
#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterWalker.h"
#include "../Camera/CameraGravityBridge.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <limits>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

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
   const GameEngine::Quaternion obbRot     = transform->transform.GetActiveQuaternion();
   const GameEngine::Vector3    halfExtents = obbHalfExtents;

   // 影響圏内の最近傍惑星を探索（OBB最近傍点距離を使用）
   int bestInRange = -1;
   float bestInRangeDist = std::numeric_limits<float>::max();
   int bestAnyIndex = 0;
   float bestAnyDist = std::numeric_limits<float>::max();

   for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
	  const auto& e = entries_[i];
	  if (!e.attractor) { continue; }

	  // OBBと惑星中心の最短距離（OBBの最近傍点→惑星中心）
	  float dist = DistancePointOBB(e.center, pos, obbRot, halfExtents);

	  if (dist < bestAnyDist) {
		 bestAnyDist = dist;
		 bestAnyIndex = i;
	  }

	  if (e.attractor->IsInRange(pos) && dist < bestInRangeDist) {
		 bestInRangeDist = dist;
		 bestInRange = i;
	  }
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

   // 切替不要なら終了
   if (newIndex == currentIndex_) { return; }
   currentIndex_ = newIndex;

   const auto& best = entries_[newIndex];

   // GravityAttractorLink の接続先を更新
   if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
	  link->SetAttractor(best.attractor);
   }

   // CharacterLanding の惑星パラメータを更新
   if (auto* landing = GetOwner().GetComponent<CharacterLanding>()) {
	  landing->SetPlanetCenter(best.center);
	  landing->surfaceRadius_ = best.surfaceRadius;
   }

   // CameraGravityBridge の惑星中心を更新
   if (auto* bridge = GetOwner().GetComponent<CameraGravityBridge>()) {
	  bridge->SetPlanetCenter(best.center);
   }

   // 切替直後の姿勢・移動破綻を防ぐためUpスナップと速度リセットを実施
   if (auto* gravityBody = GetOwner().GetComponent<GravityBody>()) {
	  GameEngine::Vector3 newUp = (pos - best.center);
	  float len = newUp.Length();
	  if (len > 1e-4f) {
		 gravityBody->SnapToUpVector(newUp * (1.0f / len));
	  }
   }
   if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
	  walker->ResetHorizontalVelocity();
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
}
#endif

} // namespace App
