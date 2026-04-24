#include "ScreenSpaceBasis.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils/QuaternionOperations.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

Vector3 ScreenSpaceBasis::ProjectOnPlane(const Vector3& v, const Vector3& normal) {
   Vector3 proj = v - normal * v.Dot(normal);
   float len = proj.Length();
   return len > 1e-4f ? proj * (1.0f / len) : Vector3{ 0.0f, 0.0f, 0.0f };
}

void ScreenSpaceBasis::UpdateBasis(const Vector3& gravityUp) {
   Vector3 screenUp    = { 0.0f, 1.0f, 0.0f };
   Vector3 screenRight = { 1.0f, 0.0f, 0.0f };

   if (gravityFollowCamera_) {
      screenUp    = gravityFollowCamera_->GetCameraUp();
      screenRight = gravityFollowCamera_->GetCameraRight();
   } else if (planetLeashCamera_) {
      screenUp    = planetLeashCamera_->GetCameraUp();
      screenRight = planetLeashCamera_->GetCameraRight();
   } else if (orbitalBody_) {
      screenUp    = orbitalBody_->GetCameraUp();
      screenRight = orbitalBody_->GetCameraRight();
   } else if (camera_) {
      Quaternion q = camera_->GetQuaternion();
      screenUp    = RotateVector({ 0.0f, 1.0f, 0.0f }, q);
      screenRight = RotateVector({ 1.0f, 0.0f, 0.0f }, q);
   } else if (auto* active = EngineContext::GetActiveCamera()) {
      Quaternion q = active->GetQuaternion();
      screenUp    = RotateVector({ 0.0f, 1.0f, 0.0f }, q);
      screenRight = RotateVector({ 1.0f, 0.0f, 0.0f }, q);
   }

   Vector3 fProj = ProjectOnPlane(screenUp,    gravityUp);
   Vector3 rProj = ProjectOnPlane(screenRight, gravityUp);

   if (fProj.LengthSquared() > 1e-6f) { cachedForward_ = fProj; }
   if (rProj.LengthSquared() > 1e-6f) { cachedRight_   = rProj; }
}

Vector3 ScreenSpaceBasis::GetForwardBasis(const Vector3& gravityUp) const {
   Vector3 f = ProjectOnPlane(cachedForward_, gravityUp);
   return f.LengthSquared() > 1e-6f ? f : cachedForward_;
}

Vector3 ScreenSpaceBasis::GetRightBasis(const Vector3& gravityUp) const {
   Vector3 r = ProjectOnPlane(cachedRight_, gravityUp);
   return r.LengthSquared() > 1e-6f ? r : cachedRight_;
}

#ifdef USE_IMGUI
void ScreenSpaceBasis::DrawInspector() {
   if (!ImGui::CollapsingHeader("ScreenSpaceBasis", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("F_proj: (%.2f, %.2f, %.2f)", cachedForward_.x, cachedForward_.y, cachedForward_.z);
   ImGui::Text("R_proj: (%.2f, %.2f, %.2f)", cachedRight_.x,   cachedRight_.y,   cachedRight_.z);
}
#endif

} // namespace App
