#include "pch.h"
#include "GravityFollowCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include "Utility/MathUtils/VectorOperations.h"
#include <algorithm>
#include <cmath>

using namespace GameEngine;

namespace App {

static Vector3 RotateAroundAxis(const Vector3& v, const Vector3& axis, float angle) {
   float c = std::cos(angle);
   float s = std::sin(angle);
   return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
}

static Vector3 ProjectOnPlaneNorm(const Vector3& v, const Vector3& up, const Vector3& fallback) {
   Vector3 proj = v - up * up.Dot(v);
   float len = proj.Length();
   return len > 1e-4f ? proj * (1.0f / len) : fallback;
}

void GravityFollowCamera::MutateCameraState(CameraState& state, float /*deltaTime*/) {
   Vector3 up = gravityUp_;
   float upLen = up.Length();
   if (upLen < 1e-6f) up = { 0.0f, 1.0f, 0.0f };
   else up = up * (1.0f / upLen);

   flatForward_ = ProjectOnPlaneNorm(flatForward_, up, flatForward_);

   Vector3 right = up.Cross(flatForward_);
   float rLen = right.Length();
   if (rLen > 1e-6f) right = right * (1.0f / rLen);
   else {
      Vector3 tmp = (std::abs(up.x) < 0.9f) ? Vector3{ 1,0,0 } : Vector3{ 0,1,0 };
      right = up.Cross(tmp);
      right = right * (1.0f / right.Length());
      flatForward_ = right.Cross(up);
      flatForward_ = flatForward_ * (1.0f / flatForward_.Length());
   }

   Vector3 pitchedForward = RotateAroundAxis(flatForward_, right, pitch_);
   float pfLen = pitchedForward.Length();
   if (pfLen > 1e-6f) pitchedForward = pitchedForward * (1.0f / pfLen);

   Vector3 eye      = pivotTarget_ + pitchedForward * (-distance_);
   Vector3 cameraUp = RotateAroundAxis(up, right, pitch_);
   float cuLen = cameraUp.Length();
   if (cuLen > 1e-6f) cameraUp = cameraUp * (1.0f / cuLen);

   Vector3 zaxis = pivotTarget_ - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) zaxis = zaxis * (1.0f / zLen);

   Vector3 xaxis = cameraUp.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) xaxis = xaxis * (1.0f / xLen);
   else xaxis = right;

   cachedRight_ = xaxis;
   cachedUp_    = zaxis.Cross(xaxis);

   state.transform.translation = eye;
   state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, cameraUp));
}

void GravityFollowCamera::ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging) {
   if (isDragging) {
      if (std::abs(mouseDelta.x) > 1e-6f) {
         Vector3 up = gravityUp_;
         float upLen = up.Length();
         if (upLen > 1e-6f) {
            up = up * (1.0f / upLen);
            flatForward_ = RotateAroundAxis(flatForward_, up, mouseDelta.x * rotateSpeed);
            float len = flatForward_.Length();
            if (len > 1e-6f) flatForward_ = flatForward_ * (1.0f / len);
         }
      }
      pitch_ -= mouseDelta.y * rotateSpeed;
      pitch_ = std::clamp(pitch_, 0.1f, 1.4f);
   }
   if (wheelDelta != 0) {
      distance_ -= wheelDelta * scrollSpeed;
      distance_ = (std::max)(1.0f, distance_);
   }
}

Vector3 GravityFollowCamera::GetCameraUp()    const { return cachedUp_; }
Vector3 GravityFollowCamera::GetCameraRight() const { return cachedRight_; }

} // namespace App
