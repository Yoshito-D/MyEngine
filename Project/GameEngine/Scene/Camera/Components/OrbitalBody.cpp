#include "pch.h"
#include "OrbitalBody.h"
#include "../Core/VirtualCamera.h"
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered = VirtualCamera::RegisterComponentFactory(
   "OrbitalBody",
   [](VirtualCamera& camera) -> ICinemachineComponent* {
	  if (auto* existing = camera.GetComponent<OrbitalBody>()) {
		 return existing;
	  }
	  return camera.AddComponent<OrbitalBody>();
   });

nlohmann::json SerializeVector3(const Vector3& value) {
   return nlohmann::json::array({ value.x, value.y, value.z });
}

Vector3 DeserializeVector3(const nlohmann::json& data, const Vector3& fallback) {
   if (!data.is_array() || data.size() != 3) {
	  return fallback;
   }
   return Vector3(data[0].get<float>(), data[1].get<float>(), data[2].get<float>());
}

float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}
} // namespace

void OrbitalBody::MutateCameraState(CameraState& state, float) {
   rotationMatrix_ = MakeRotateXMatrix(pitch_) * MakeRotateYMatrix(yaw_);

   Vector3 offset = TransformCoordinate({ 0.0f, 0.0f, distance_ }, rotationMatrix_);
   Vector3 eye = pivotTarget_ + offset;
   Vector3 up = TransformNormal({ 0.0f, 1.0f, 0.0f }, rotationMatrix_);

   // 元のDebugCameraと完全に同じ計算でビュー行列を直接生成する
   // Transform→Quaternion→再構築の変換パスを使わない
   state.transform.translation = eye;
   state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, up));
}

void OrbitalBody::ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta,
   bool isDragging, bool isShiftPressed) {
   if (isDragging) {
	  if (isShiftPressed) {
		 // パン操作
		 const float cosPitch = std::cos(pitch_);
		 const float sinPitch = std::sin(pitch_);
		 const float cosYaw = std::cos(yaw_);
		 const float sinYaw = std::sin(yaw_);

		 Vector3 offset = {
			 distance_ * cosPitch * sinYaw,
			 -distance_ * sinPitch,
			 distance_ * cosPitch * cosYaw
		 };

		 Vector3 forward = (-offset).Normalize();
		 Vector3 right = Vector3(0.0f, 1.0f, 0.0f).Cross(forward);
		 if (right.LengthSquared() > 1e-6f) {
			right = right.Normalize();
		 } else {
			right = { 1.0f, 0.0f, 0.0f };
		 }
		 Vector3 up = forward.Cross(right).Normalize();

		 float actualMoveSpeed = moveSpeed_ * distance_;

		 pivotTarget_ = pivotTarget_ - right * (mouseDelta.x * actualMoveSpeed);
		 pivotTarget_ = pivotTarget_ + up * (mouseDelta.y * actualMoveSpeed);
	  } else {
		 // 回転操作
		 yaw_ += mouseDelta.x * rotateSpeed_;
		 pitch_ -= mouseDelta.y * rotateSpeed_;

		 // ピッチの制限（真上・真下を防ぐ）
		 constexpr float kPitchLimit = 1.5f; // 約86度
		 pitch_ = std::clamp(pitch_, -kPitchLimit, kPitchLimit);
	  }
   }

   // ズーム操作
   if (wheelDelta != 0) {
	  distance_ -= wheelDelta * scrollSpeed_;
	  distance_ = (std::max)(0.5f, distance_);
   }
}

#pragma region Serialization
nlohmann::json OrbitalBody::Serialize() const {
   return nlohmann::json{
	   { "yaw", yaw_ },
	   { "pitch", pitch_ },
	   { "distance", distance_ },
	   { "pivotTarget", SerializeVector3(pivotTarget_) },
	   { "rotateSpeed", rotateSpeed_ },
	   { "scrollSpeed", scrollSpeed_ },
	   { "moveSpeed", moveSpeed_ }
   };
}

void OrbitalBody::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   yaw_ = ReadFloat(data, "yaw", yaw_);
   pitch_ = ReadFloat(data, "pitch", pitch_);
   SetDistance(ReadFloat(data, "distance", distance_));
   if (data.contains("pivotTarget")) {
	  pivotTarget_ = DeserializeVector3(data.at("pivotTarget"), pivotTarget_);
   }
   rotateSpeed_ = ReadFloat(data, "rotateSpeed", rotateSpeed_);
   scrollSpeed_ = ReadFloat(data, "scrollSpeed", scrollSpeed_);
   moveSpeed_ = ReadFloat(data, "moveSpeed", moveSpeed_);
}
#pragma endregion

#ifdef USE_IMGUI
static constexpr float kRadToDeg = 57.2957795f;
static constexpr float kDegToRad = 1.0f / kRadToDeg;

void OrbitalBody::DrawInspector() {
   if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

   if (ImGui::DragFloat("Distance", &distance_, 0.1f, 0.5f, 1000.0f)) {
	  distance_ = (std::max)(0.5f, distance_);
   }

   float yawDeg = yaw_ * kRadToDeg;
   if (ImGui::SliderFloat("Yaw (deg)", &yawDeg, -180.0f, 180.0f)) {
	  yaw_ = yawDeg * kDegToRad;
   }

   float pitchDeg = pitch_ * kRadToDeg;
   if (ImGui::SliderFloat("Pitch (deg)", &pitchDeg, -89.0f, 89.0f)) {
	  pitch_ = pitchDeg * kDegToRad;
   }

   float piv[3] = { pivotTarget_.x, pivotTarget_.y, pivotTarget_.z };
   if (ImGui::DragFloat3("Pivot Target", piv, 0.1f)) {
	  pivotTarget_ = { piv[0], piv[1], piv[2] };
   }
}
#endif

} // namespace GameEngine
