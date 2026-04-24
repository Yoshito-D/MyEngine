#include "PlanetSwitcher.h"
#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "../Character/CharacterLanding.h"
#include "../Character/CharacterWalker.h"
#include "../Camera/CameraGravityBridge.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include <limits>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void PlanetSwitcher::Update(float /*deltaTime*/) {
   if (!HasOwner() || entries_.empty()) { return; }

   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }

   const GameEngine::Vector3 pos = transform->transform.translation;

   // 影響圏内かつ最近傍を探す。影響圏外なら全惑星の中で最近傍にフォールバック
   int bestInRange = -1;
   float bestInRangeDist = std::numeric_limits<float>::max();
   int bestAnyIndex = 0;
   float bestAnyDist = std::numeric_limits<float>::max();

   for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
	  const auto& e = entries_[i];
	  if (!e.attractor) { continue; }

	  float dist = (pos - e.center).Length();

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

   if (newIndex == currentIndex_) { return; }
   currentIndex_ = newIndex;

   const auto& best = entries_[newIndex];

   // GravityAttractorLink を切り替え
   if (auto* link = GetOwner().GetComponent<GravityAttractorLink>()) {
	  link->SetAttractor(best.attractor);
   }

   // CharacterLanding を切り替え
   if (auto* landing = GetOwner().GetComponent<CharacterLanding>()) {
	  landing->SetPlanetCenter(best.center);
	  landing->surfaceRadius_ = best.surfaceRadius;
   }

   // CameraGravityBridge を切り替え
   if (auto* bridge = GetOwner().GetComponent<CameraGravityBridge>()) {
	  bridge->SetPlanetCenter(best.center);
   }

   // 惑星切替時の姿勢バグ修正: 新しい重力方向へ即時スナップ & 水平速度をリセット
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
   if (!ImGui::CollapsingHeader("PlanetSwitcher", ImGuiTreeNodeFlags_DefaultOpen)) {
	  return;
   }
   ImGui::Separator();
   ImGui::Text("Planets: %d", static_cast<int>(entries_.size()));
   ImGui::Text("Current Planet Index: %d", currentIndex_);
}
#endif

} // namespace App
