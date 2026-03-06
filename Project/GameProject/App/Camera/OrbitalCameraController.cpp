#include "OrbitalCameraController.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils.h"
#include <cmath>
#include <algorithm>

using namespace GameEngine;

OrbitalCameraController::OrbitalCameraController(GameEngine::Camera* camera)
   : camera_(camera) {
}

void OrbitalCameraController::SetOrbitParams(const OrbitParams& params) {
   params_ = params;
}

void OrbitalCameraController::Start(bool loop) {
   if (!camera_) {
	  return;
   }

   isMoving_ = true;
   isPaused_ = false;
   isCompleted_ = false;
   isLooping_ = loop;
   currentTime_ = 0.0f;
   currentAngleY_ = params_.startAngleY;
   currentAngleX_ = params_.startAngleX;
   currentRadius_ = params_.radius;  // 初期radiusを設定

   // 初期位置を計算
   currentPosition_ = SphericalToCartesian(
	  currentRadius_,
	  currentAngleY_,
	  currentAngleX_,
	  params_.targetPosition
   );

   // カメラに適用
   camera_->SetPosition(currentPosition_);
   camera_->SetFovY(params_.fov);

   if (params_.lookAtTarget) {
	  Vector3 rotation = CalculateLookAtRotation(currentPosition_, params_.targetPosition);
	  camera_->SetRotation(rotation);
   }

   camera_->Update();
}

void OrbitalCameraController::Stop() {
   isMoving_ = false;
   isPaused_ = false;
   isCompleted_ = false;
   currentTime_ = 0.0f;
}

void OrbitalCameraController::Pause() {
   if (isMoving_) {
	  isPaused_ = true;
   }
}

void OrbitalCameraController::Resume() {
   if (isPaused_) {
	  isPaused_ = false;
   }
}

void OrbitalCameraController::Reset() {
   Stop();
   currentAngleY_ = params_.startAngleY;
   currentAngleX_ = params_.startAngleX;
}

void OrbitalCameraController::Update() {
   if (!isMoving_ || isPaused_ || !camera_) {
	  return;
   }

   float deltaTime = GameEngine::EngineContext::GetDeltaTime();
   currentTime_ += deltaTime;

   // 進行度を計算
   float t = std::clamp(currentTime_ / params_.duration, 0.0f, 1.0f);

   // イージングを適用
   float easedT = CameraKeyframe::ApplyEasing(t, params_.easingType, params_.easingPower);

   // 角度を補間
   currentAngleY_ = params_.startAngleY + (params_.endAngleY - params_.startAngleY) * easedT;
   currentAngleX_ = params_.startAngleX + (params_.endAngleX - params_.startAngleX) * easedT;

   // 位置を計算（currentRadius_を使用）
   currentPosition_ = SphericalToCartesian(
	  currentRadius_,
	  currentAngleY_,
	  currentAngleX_,
	  params_.targetPosition
   );

   // カメラに適用
   camera_->SetPosition(currentPosition_);

   if (params_.lookAtTarget) {
	  Vector3 rotation = CalculateLookAtRotation(currentPosition_, params_.targetPosition);
	  camera_->SetRotation(rotation);
   }

   camera_->Update();

   // 完了判定
   if (t >= 1.0f) {
	  if (isLooping_) {
		 // ループ
		 currentTime_ = 0.0f;
		 currentAngleY_ = params_.startAngleY;
		 currentAngleX_ = params_.startAngleX;
		 currentRadius_ = params_.radius;
	  } else {
		 // 完了
		 isMoving_ = false;
		 isCompleted_ = true;
		 
		 if (onCompleteCallback_) {
			onCompleteCallback_();
		 }
	  }
   }
}

float OrbitalCameraController::GetProgress() const {
   if (params_.duration <= 0.0f) {
	  return 1.0f;
   }
   return std::clamp(currentTime_ / params_.duration, 0.0f, 1.0f);
}

Vector3 OrbitalCameraController::SphericalToCartesian(float radius, float angleY, float angleX, const Vector3& center) {
   // 球面座標から直交座標への変換
   // angleY: Y軸周りの角度（水平方向）
   // angleX: X軸周りの角度（垂直方向）
   
   float x = radius * std::cos(angleX) * std::sin(angleY);
   float y = radius * std::sin(angleX);
   float z = radius * std::cos(angleX) * std::cos(angleY);

   return Vector3(
	  center.x + x,
	  center.y + y,
	  center.z + z
   );
}

Vector3 OrbitalCameraController::CalculateLookAtRotation(const Vector3& position, const Vector3& target) {
   // ターゲットへの方向ベクトルを計算
   Vector3 direction = target - position;
   direction = Normalize(direction);

   // Y軸周りの回転（yaw）を計算
   float yaw = std::atan2(direction.x, direction.z);

   // X軸周りの回転（pitch）を計算
   float horizontalDistance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
   float pitch = -std::atan2(direction.y, horizontalDistance);

   // Euler角として返す
   return Vector3(pitch, yaw, 0.0f);
}
