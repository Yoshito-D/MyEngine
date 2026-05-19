#include "VehicleLandingBoost.h"
#include "VehicleGroundMover.h"
#include "Object/Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

bool VehicleLandingBoost::TryBoost(const Vector3& localUp, const Vector3& gravityUp) {
   // 着地時の機体上向き (localUp) と重力上向き (gravityUp) の一致度を内積で測る。
   // 完全に一致していれば 1.0、完全にズレていれば 0.0 になる。
   // clamp で負値（真逆方向）を 0 に丸める。
   float alignment = std::clamp(localUp.Dot(gravityUp), 0.0f, 1.0f);

   // alignment が閾値を下回る（機体が斜めで着地）場合はブーストしない。
   // 逆さまやひどく傾いた着地にボーナスを与えないための安全弁。
   if (alignment < boostThreshold) { return false; }

   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) { return false; }

   // autoSpeed を基準として boostAmount を加算する。
   // autoSpeed より遅かった場合（ブレーキ直後など）も含め、
   // 着地ボーナスが常に同じ量になるよう基準値からの加算にしている。
   float baseSpeed = groundMover->autoSpeed;
   groundMover->SetCurrentSpeed(baseSpeed + boostAmount);
   return true;
}

#ifdef USE_IMGUI
void VehicleLandingBoost::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleLandingBoost")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Boost Amount",    &boostAmount,    0.1f, 0.0f, 200.0f);
   ImGui::DragFloat("Boost Threshold", &boostThreshold, 0.01f, 0.0f,   1.0f);
}
#endif

nlohmann::json VehicleLandingBoost::Serialize() const {
   nlohmann::json json;
   json["boostAmount"]    = boostAmount;
   json["boostThreshold"] = boostThreshold;
   return json;
}

void VehicleLandingBoost::Deserialize(const nlohmann::json& data) {
   if (data.contains("boostAmount"))    { boostAmount    = data["boostAmount"]; }
   if (data.contains("boostThreshold")) { boostThreshold = data["boostThreshold"]; }
}

} // namespace App
