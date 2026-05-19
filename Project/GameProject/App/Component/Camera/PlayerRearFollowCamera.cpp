#include "PlayerRearFollowCamera.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace App {

void PlayerRearFollowCamera::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
	using namespace GameEngine;

	// upVectorは常に惑星基準（gravityUp_）を使用する
	Vector3 up = gravityUp_;
	float upLen = up.Length();
	if (upLen < 1e-6f) {
		up = { 0.0f, 1.0f, 0.0f };
	} else {
		up = up * (1.0f / upLen);
	}

	Vector3 desiredBackward = ProjectOnPlaneNorm(-followForward_, up, currentBackward_);

	if (!isInitialized_) {
		// 初回はそのまま設定
		currentBackward_ = desiredBackward;
		isInitialized_ = true;
	} else {
		// GravityFollowCamera と同様、まず現在後方を重力平面へ再投影して
		// 惑星基準upの変化に追従させる
		currentBackward_ = ProjectOnPlaneNorm(currentBackward_, up, desiredBackward);

		if (!isAirborne_) {
			// 地上時のみヨー（水平後方）を補間追従する。空中時は維持。
			float t = std::clamp(rearLerpSpeed * deltaTime, 0.0f, 1.0f);
			currentBackward_ = Vector3::Lerp(currentBackward_, desiredBackward, t).Normalize();
			if (currentBackward_.LengthSquared() < 1e-6f) {
				currentBackward_ = desiredBackward;
			}
		}
	}

	// upは惑星基準のまま eye を算出
	Vector3 eye = pivotTarget_ + up * height + currentBackward_ * distance;

	Vector3 zaxis = pivotTarget_ - eye;
	float zLen = zaxis.Length();
	if (zLen > 1e-6f) {
		zaxis = zaxis * (1.0f / zLen);
	} else {
		zaxis = -currentBackward_;
	}

	Vector3 xaxis = up.Cross(zaxis);
	float xLen = xaxis.Length();
	if (xLen > 1e-6f) {
		xaxis = xaxis * (1.0f / xLen);
	} else {
		xaxis = cachedRight_;
	}

	cachedRight_ = xaxis;
	// GetCameraUp() は惑星基準のupを返すため、gravityUp_の正規化済み値を保持する
	cachedUp_ = up;

	state.transform.translation = eye;
	// LookAt の up も惑星基準
	state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, up));
}

GameEngine::Vector3 PlayerRearFollowCamera::ProjectOnPlaneNorm(const GameEngine::Vector3& v,
															   const GameEngine::Vector3& up,
															   const GameEngine::Vector3& fallback) {
	using namespace GameEngine;

	Vector3 proj = v - up * up.Dot(v);
	float len = proj.Length();
	if (len > 1e-4f) {
		return proj * (1.0f / len);
	}

	Vector3 fb = fallback - up * up.Dot(fallback);
	float fbLen = fb.Length();
	if (fbLen > 1e-4f) {
		return fb * (1.0f / fbLen);
	}

	Vector3 axis = (std::abs(up.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
	Vector3 out = axis - up * up.Dot(axis);
	float outLen = out.Length();
	return outLen > 1e-4f ? out * (1.0f / outLen) : Vector3{ 0.0f, 0.0f, 1.0f };
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
	if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

	ImGui::DragFloat("Distance", &distance, 0.1f, 1.0f, 100.0f);
	ImGui::DragFloat("Height", &height, 0.1f, -20.0f, 50.0f);
	ImGui::DragFloat("Rear Lerp Speed", &rearLerpSpeed, 0.1f, 0.0f, 30.0f);

	ImGui::Separator();
	ImGui::Text("Airborne: %s", isAirborne_ ? "true" : "false");
	ImGui::Text("Gravity Up:    (%.2f, %.2f, %.2f)", gravityUp_.x, gravityUp_.y, gravityUp_.z);
	ImGui::Text("FollowForward: (%.2f, %.2f, %.2f)", followForward_.x, followForward_.y, followForward_.z);
}
#endif

} // namespace App
