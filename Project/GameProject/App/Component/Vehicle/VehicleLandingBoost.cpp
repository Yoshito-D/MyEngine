#include "VehicleLandingBoost.h"
#include "VehicleGroundMover.h"
#include "Object/Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

LandingResult VehicleLandingBoost::TryBoost(const Vector3& localUp, const Vector3& gravityUp) {
   // 着地時の機体上向き (localUp) と重力上向き (gravityUp) の一致度を内積で測る。
   // 完全に一致していれば 1.0、完全にズレていれば 0.0 になる。
   // clamp で負値（真逆方向）を 0 に丸める。
   float alignment = std::clamp(localUp.Dot(gravityUp), 0.0f, 1.0f);

   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) { return LandingResult::Normal; }

   if (alignment >= boostThreshold) {
      // 成功: 綺麗に着地したのでブーストを加算する。
      groundMover->AddVelocityImpulse(boostAmount);
      return LandingResult::Success;
   }
   if (alignment >= normalThreshold) {
      // 普通: 多少傾いた着地なので速度は変化させない。
      return LandingResult::Normal;
   }
   // 失敗: 大きく傾いた着地なので速度を penaltySpeed に設定する。
   groundMover->SetCurrentSpeed(penaltySpeed);
   return LandingResult::Failure;
}

#ifdef USE_IMGUI
void VehicleLandingBoost::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleLandingBoost")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Boost Amount",      &boostAmount,     0.1f,  0.0f, 200.0f);
   ImGui::DragFloat("Boost Threshold",  &boostThreshold,  0.01f, 0.0f,   1.0f);
   ImGui::DragFloat("Normal Threshold", &normalThreshold, 0.01f, 0.0f,   1.0f);
   ImGui::DragFloat("Penalty Speed",    &penaltySpeed,    0.1f,  0.0f, 200.0f);
}
#endif

nlohmann::json VehicleLandingBoost::Serialize() const {
   nlohmann::json json;
   json["boostAmount"]     = boostAmount;
   json["boostThreshold"]  = boostThreshold;
   json["normalThreshold"] = normalThreshold;
   json["penaltySpeed"]    = penaltySpeed;
   return json;
}

void VehicleLandingBoost::Deserialize(const nlohmann::json& data) {
   if (data.contains("boostAmount"))     { boostAmount     = data["boostAmount"]; }
   if (data.contains("boostThreshold"))  { boostThreshold  = data["boostThreshold"]; }
   if (data.contains("normalThreshold")) { normalThreshold = data["normalThreshold"]; }
   if (data.contains("penaltySpeed"))    { penaltySpeed    = data["penaltySpeed"]; }
}

} // namespace App
