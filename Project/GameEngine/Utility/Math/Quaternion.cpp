#include "Quaternion.h"
#include "Vector3.h"

GameEngine::Vector3 GameEngine::Quaternion::ToEuler() const {
	// pitch（X軸回転）
	float ysqr = y * y;
	float t0 = +2.0f * (w * x + y * z);
	float t1 = +1.0f - 2.0f * (x * x + ysqr);
	// yaw（Y軸回転）
	float t2 = +2.0f * (w * y - z * x);
	t2 = std::clamp(t2, -1.0f, 1.0f);
	float t3 = +2.0f * (w * z + x * y);
	float t4 = +1.0f - 2.0f * (ysqr + z * z);
	// roll（Z軸回転）
	float t4_clamped = std::clamp(t4, -1.0f, 1.0f);
	float t2_clamped = std::clamp(t2, -1.0f, 1.0f);
	float pitch = std::atan2(t0, t1);
	float yaw = std::asin(t2_clamped);
	float roll = std::atan2(t3, t4_clamped);
	return Vector3(pitch, yaw, roll);
}
