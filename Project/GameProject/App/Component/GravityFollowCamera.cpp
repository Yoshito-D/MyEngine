#include "pch.h"
#include "GravityFollowCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include "Utility/MathUtils/VectorOperations.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {

// gravityUp軸周りに v を angle ラジアン回転する（Rodrigues）
static Vector3 RotateAroundAxis(const Vector3& v, const Vector3& axis, float angle) {
   float c = std::cos(angle);
   float s = std::sin(angle);
   return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
}

// v を gravityUp に垂直な平面に投影して正規化（失敗時は fallback を返す）
static Vector3 ProjectOnPlaneNorm(const Vector3& v, const Vector3& up, const Vector3& fallback) {
   Vector3 proj = v - up * up.Dot(v);
   float len = proj.Length();
   return len > 1e-4f ? proj * (1.0f / len) : fallback;
}

void GravityFollowCamera::MutateCameraState(CameraState& state, float /*deltaTime*/) {
   // --- gravityUp を正規化 ---
   Vector3 up = gravityUp_;
   float upLen = up.Length();
   if (upLen < 1e-6f) up = { 0.0f, 1.0f, 0.0f };
   else up = up * (1.0f / upLen);

   // ========================================================
   // flatForward_ を現在の gravityUp 平面に再投影
   //
   // プレイヤーが惑星上を移動すると gravityUp が変化する。
   // flatForward_ をその都度平面に投影し直すことで、
   // 固定基準ベクトルを一切使わずに連続した前方向を維持する。
   // ========================================================
   flatForward_ = ProjectOnPlaneNorm(flatForward_, up, flatForward_);

   // --- right = up × flatForward（左手系） ---
   Vector3 right = up.Cross(flatForward_);
   float rLen = right.Length();
   if (rLen > 1e-6f) right = right * (1.0f / rLen);
   else {
	  // flatForward_ が up と平行な縮退ケース（極めて稀）
	  // 任意の直交ベクトルを生成して継続
	  Vector3 tmp = (std::abs(up.x) < 0.9f) ? Vector3{ 1,0,0 } : Vector3{ 0,1,0 };
	  right = up.Cross(tmp);
	  right = right * (1.0f / right.Length());
	  flatForward_ = right.Cross(up);
	  flatForward_ = flatForward_ * (1.0f / flatForward_.Length());
   }

   // ========================================================
   // Pitch 回転: right 軸周りに flatForward を仰角回転
   //   pitch_ > 0 → 上から見下ろす（カメラが上に上がる）
   // ========================================================
   Vector3 pitchedForward = RotateAroundAxis(flatForward_, right, pitch_);
   float pfLen = pitchedForward.Length();
   if (pfLen > 1e-6f) pitchedForward = pitchedForward * (1.0f / pfLen);

   // --- eye 位置（pivot の後ろ上方） ---
   Vector3 eye = pivotTarget_ + pitchedForward * (-distance_);

   // --- cameraUp: pitch 後の up を right 軸で同角度回転 ---
   Vector3 cameraUp = RotateAroundAxis(up, right, pitch_);
   float cuLen = cameraUp.Length();
   if (cuLen > 1e-6f) cameraUp = cameraUp * (1.0f / cuLen);

   // ========================================================
   // スクリーン軸をキャッシュ（PlayerController のスクリーンスペース投影用）
   // MakeLookAtMatrix と同じ計算で xaxis/yaxis を求める
   //   zaxis = normalize(target - eye)
   //   xaxis = normalize(cameraUp × zaxis)   ← left-hand
   //   yaxis = zaxis × xaxis
   // ========================================================
   Vector3 zaxis = pivotTarget_ - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) zaxis = zaxis * (1.0f / zLen);

   Vector3 xaxis = cameraUp.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) xaxis = xaxis * (1.0f / xLen);
   else xaxis = right; // フォールバック

   Vector3 yaxis = zaxis.Cross(xaxis);

   cachedRight_ = xaxis;
   cachedUp_ = yaxis;

   // --- ビュー行列を直接設定 ---
   state.transform.translation = eye;
   state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, cameraUp));
}

void GravityFollowCamera::ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging) {
   if (isDragging) {
	  // ========================================================
	  // Yaw: flatForward_ を gravityUp 軸周りに直接回転
	  //   固定角度の代わりに差分回転を毎フレーム適用する。
	  //   これにより基準ベクトルの切り替えジャンプが発生しない。
	  // ========================================================
	  if (std::abs(mouseDelta.x) > 1e-6f) {
		 Vector3 up = gravityUp_;
		 float upLen = up.Length();
		 if (upLen > 1e-6f) {
			up = up * (1.0f / upLen);
			flatForward_ = RotateAroundAxis(flatForward_, up, mouseDelta.x * rotateSpeed);
			// 念のため再正規化
			float len = flatForward_.Length();
			if (len > 1e-6f) flatForward_ = flatForward_ * (1.0f / len);
		 }
	  }

	  // Pitch: 角度をクランプして保存
	  pitch_ -= mouseDelta.y * rotateSpeed;
	  constexpr float kPitchMin = 0.1f;  // 最小：ほぼ水平
	  constexpr float kPitchMax = 1.4f;  // 最大：ほぼ真上から
	  pitch_ = std::clamp(pitch_, kPitchMin, kPitchMax);
   }

   if (wheelDelta != 0) {
	  distance_ -= wheelDelta * scrollSpeed;
	  distance_ = (std::max)(1.0f, distance_);
   }
}

Vector3 GravityFollowCamera::GetCameraUp() const {
   return cachedUp_;
}

Vector3 GravityFollowCamera::GetCameraRight() const {
   return cachedRight_;
}

} // namespace GameEngine
