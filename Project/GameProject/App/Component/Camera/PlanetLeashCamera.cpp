#include "pch.h"
#include "PlanetLeashCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

using namespace GameEngine;

namespace App {

void PlanetLeashCamera::MutateCameraState(CameraState& state, float deltaTime) {
   if (!isInitialized_) {
      eyePos_        = state.transform.translation;
      prevGravityUp_ = gravityUp_;
      eyeRelUp_      = gravityUp_;
      isInitialized_ = true;
   }

   // gravityUp 差分回転を eyePos_ / eyeRelUp_ に適用（ロール防止）
   {
      Vector3 up0 = prevGravityUp_;
      Vector3 up1 = gravityUp_;
      float u0Len = up0.Length(), u1Len = up1.Length();
      if (u0Len > 1e-6f && u1Len > 1e-6f) {
         up0 = up0 * (1.0f / u0Len);
         up1 = up1 * (1.0f / u1Len);
         float cosA = std::clamp(up0.Dot(up1), -1.0f, 1.0f);
         if (cosA < 1.0f - 1e-7f) {
            Vector3 axis = up0.Cross(up1);
            float axLen = axis.Length();
            if (axLen > 1e-6f) {
               axis = axis * (1.0f / axLen);
               float angle = std::acos(cosA);
               float c = std::cos(angle), s = std::sin(angle);
               auto rodrigues = [&](const Vector3& v) -> Vector3 {
                  return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
               };
               Vector3 r = eyePos_ - pivotTarget_;
               eyePos_ = pivotTarget_ + rodrigues(r);
               eyeRelUp_ = rodrigues(eyeRelUp_);
               float upNLen = eyeRelUp_.Length();
               if (upNLen > 1e-6f) eyeRelUp_ = eyeRelUp_ * (1.0f / upNLen);
            }
         }
      }
      prevGravityUp_ = gravityUp_;
   }

   // レアッシュ
   Vector3 toTarget = pivotTarget_ - eyePos_;
   float dist = toTarget.Length();
   if (dist > maxFollowDistance) {
      float over = dist - maxFollowDistance;
      float move = (std::min)(over, followSpeed * deltaTime);
      eyePos_ = eyePos_ + toTarget * (move / dist);
   }

   // 惑星クランプ
   Vector3 fromCenter = eyePos_ - sphereCenter_;
   float fromCenterDist = fromCenter.Length();
   if (fromCenterDist < minPlanetDistance && fromCenterDist > 1e-6f) {
      eyePos_ = sphereCenter_ + fromCenter * (minPlanetDistance / fromCenterDist);
   }

   // LookAt
   Vector3 lookDir = pivotTarget_ - eyePos_;
   float lookLen = lookDir.Length();
   if (lookLen < 1e-6f) { return; }
   Vector3 lookDirN = lookDir * (1.0f / lookLen);

   {
      Vector3 projected = eyeRelUp_ - lookDirN * eyeRelUp_.Dot(lookDirN);
      float pLen = projected.Length();
      if (pLen > 1e-6f) {
         eyeRelUp_ = projected * (1.0f / pLen);
      } else {
         Vector3 tmp = (std::abs(lookDirN.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f }
                                                       : Vector3{ 0.0f, 1.0f, 0.0f };
         tmp = tmp - lookDirN * tmp.Dot(lookDirN);
         eyeRelUp_ = tmp * (1.0f / tmp.Length());
      }
   }

   Vector3 zaxis = lookDirN;
   Vector3 xaxis = eyeRelUp_.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) {
      cachedRight_ = xaxis * (1.0f / xLen);
      cachedUp_    = zaxis.Cross(cachedRight_);
   }

   state.transform.translation = eyePos_;
   state.SetViewMatrix(MakeLookAtMatrix(eyePos_, pivotTarget_, eyeRelUp_));
}

#ifdef USE_IMGUI
void PlanetLeashCamera::DrawInspector() {
   if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

   ImGui::DragFloat("Max Follow Distance", &maxFollowDistance, 0.1f, 0.1f, 100.0f);
   ImGui::DragFloat("Follow Speed",        &followSpeed,       0.1f, 0.0f, 50.0f);
   ImGui::DragFloat("Min Planet Distance", &minPlanetDistance, 0.1f, 0.0f, 100.0f);
   ImGui::Checkbox("Use Gravity Up", &useGravityUp);

   ImGui::Separator();
   ImGui::Text("Eye Pos:   (%.2f, %.2f, %.2f)", eyePos_.x, eyePos_.y, eyePos_.z);
   ImGui::Text("Gravity Up:(%.2f, %.2f, %.2f)", gravityUp_.x, gravityUp_.y, gravityUp_.z);
   ImGui::Text("Pivot:     (%.2f, %.2f, %.2f)", pivotTarget_.x, pivotTarget_.y, pivotTarget_.z);
   ImGui::Text("Sphere Ctr:(%.2f, %.2f, %.2f)", sphereCenter_.x, sphereCenter_.y, sphereCenter_.z);

   if (ImGui::Button("Reset Initialization")) {
      isInitialized_ = false;
   }
}
#endif

} // namespace App
