#pragma once
#include "RearCameraTypes.h"
#include "Utility/MathUtils/MathConstants.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace App::RearCameraMath {

inline constexpr float kPlanetVisibilityGainFadeRange = 0.08f;
inline constexpr float kDefaultSafeFov = 0.45f;
inline constexpr float kMinSafeFov = 0.017453292f;  // 1 degree
inline constexpr float kMaxSafeFov = 3.12413936f;   // 179 degrees
inline constexpr float kMaxSpringDeltaTime = 0.25f;
inline constexpr float kMaxSpringStep = 1.0f / 120.0f;
// 旧ターボ係数が最大になっていた速度差を統合後も基準として使う。
inline constexpr float kSpeedDeltaForMaxKick = 5.0f;
inline constexpr float kGroundDirectionProjectionBlendRange = 0.15f;
inline constexpr float kRadiansToDegrees = 57.29577951308232f;
inline constexpr size_t kMaxCameraMeasurementSamples = 36000;
inline constexpr const char* kCameraMeasurementDirectory = "../Generated/CameraMeasurements";

/// @brief 非有限値を既定値へ戻し、描画に使えるFOV範囲へ制限する。
inline float ClampCameraFov(float fov) {
   if (!std::isfinite(fov)) {
	  return kDefaultSafeFov;
   }
   return std::clamp(fov, kMinSafeFov, kMaxSafeFov);
}

/// @brief 指数平滑の補間係数を返す（dt 変動に強く、常に 0..1 未満）
inline float ExpSmoothingFactor(float speed, float deltaTime) {
   float k = std::max(0.0f, speed);
   float dt = std::max(0.0f, deltaTime);
   return 1.0f - std::exp(-k * dt);
}

/// @brief JSONの数値を読み、欠落や型違いでは既存設定を維持する。
inline float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}

/// @brief JSONの真偽値を読み、欠落や型違いでは既存設定を維持する。
inline bool ReadBool(const nlohmann::json& data, const char* key, bool fallback) {
   return data.contains(key) && data.at(key).is_boolean() ? data.at(key).get<bool>() : fallback;
}

/// @brief 微小ベクトルでは代替方向を使い、正規化可能な方向を返す。
inline GameEngine::Vector3 NormalizeOrFallback(
   const GameEngine::Vector3& value,
   const GameEngine::Vector3& fallback) {
   float len = value.Length();
   if (len > 1e-5f) {
	  return value * (1.0f / len);
   }

   float fallbackLen = fallback.Length();
   if (fallbackLen > 1e-5f) {
	  return fallback * (1.0f / fallbackLen);
   }

   return { 0.0f, 0.0f, 1.0f };
}

/// @brief 1次元Spring（半陰的オイラー）で状態を1ステップ進める
inline void StepSpring1D(float target,
   float stiffness,
   float damping,
   float deltaTime,
   float& inOutValue,
   float& inOutVelocity) {
   float dt = std::clamp(deltaTime, 0.0f, kMaxSpringDeltaTime);
   if (dt <= 0.0f) return;

   float k = std::max(0.0f, stiffness);
   float c = std::max(0.0f, damping);

   if (!std::isfinite(inOutValue)) {
	  inOutValue = target;
   }
   if (!std::isfinite(inOutVelocity)) {
	  inOutVelocity = 0.0f;
   }

   // 起動直後やブレーク復帰時の大きな dt をそのまま入れると、半陰的オイラーでも
   // ばね速度が反転し過ぎて FOV オフセットが負方向へ大きく飛ぶため、小刻みに積分する。
   while (dt > 0.0f) {
	  float step = std::min(dt, kMaxSpringStep);
	  float accel = -k * (inOutValue - target) - c * inOutVelocity;
	  inOutVelocity += accel * step;
	  inOutValue += inOutVelocity * step;
	  dt -= step;

	  if (!std::isfinite(inOutValue) || !std::isfinite(inOutVelocity)) {
		 inOutValue = target;
		 inOutVelocity = 0.0f;
		 return;
	  }
   }
}

/// @brief 現在ベクトルを目標ベクトルへ「最大角速度」で回転させる
/// @details 180°近傍でもゼロベクトル化を避けるため、線形補間ではなく角度ベースで追従する。
inline GameEngine::Vector3 RotateTowardsUnit(const GameEngine::Vector3& current,
   const GameEngine::Vector3& target,
   float maxRadiansDelta,
   const GameEngine::Vector3& preferredAntipodalAxis = { 0.0f, 0.0f, 0.0f }) {
   using namespace GameEngine;

   Vector3 c = current;
   float cLen = c.Length();
   if (cLen < 1e-6f) c = { 0.0f, 1.0f, 0.0f };
   else c = c * (1.0f / cLen);

   Vector3 t = target;
   float tLen = t.Length();
   if (tLen < 1e-6f) t = { 0.0f, 1.0f, 0.0f };
   else t = t * (1.0f / tLen);

   float dot = std::clamp(c.Dot(t), -1.0f, 1.0f);
   float angle = std::acos(dot);
   if (angle < 1e-6f) {
	  return t;
   }

   float step = std::max(0.0f, maxRadiansDelta);
   if (step >= angle) {
	  return t;
   }

   if (dot < -0.999f) {
	  // 180度では回転軸が一意に決まらない。Upの反転時は表示中のRightを渡し、
	  // 任意のワールド軸が選ばれて急なロール経路へ入ることを防ぐ。
	  Vector3 axis =
		 preferredAntipodalAxis
		 - c * c.Dot(preferredAntipodalAxis);
	  float axisLen = axis.Length();
	  if (axisLen < 1e-6f) {
		 Vector3 fallbackAxis = (std::abs(c.x) < 0.9f)
			? Vector3{ 1.0f, 0.0f, 0.0f }
			: Vector3{ 0.0f, 1.0f, 0.0f };
		 axis = c.Cross(fallbackAxis);
		 axisLen = axis.Length();
	  }
	  axis = axisLen > 1e-6f
		 ? axis * (1.0f / axisLen)
		 : Vector3{ 0.0f, 0.0f, 1.0f };

	  // ロドリゲス回転
	  float cs = std::cos(step);
	  float sn = std::sin(step);
	  return c * cs + axis.Cross(c) * sn + axis * (axis.Dot(c) * (1.0f - cs));
   }

   float tRatio = step / angle;
   float sinTotal = std::sin(angle);
   float w0 = std::sin((1.0f - tRatio) * angle) / sinTotal;
   float w1 = std::sin(tRatio * angle) / sinTotal;
   Vector3 out = c * w0 + t * w1;
   float outLen = out.Length();
   return outLen > 1e-6f ? out * (1.0f / outLen) : t;
}

/// @brief 単位方向同士を角度ベースで補間する
/// @details 方向ベクトルを通常の Lerp で混ぜると、180°近い組み合わせで途中がゼロに近づき、
///          Normalize 後に反対方向へ跳ぶため、カメラの前後反転として見える。
///          角度量へ変換して RotateTowardsUnit することで、必ず現在方向から目標方向へ連続回転させる。
inline GameEngine::Vector3 BlendUnitDirectionSafely(const GameEngine::Vector3& current,
   const GameEngine::Vector3& target,
   float blend,
   const GameEngine::Vector3& fallback,
   const GameEngine::Vector3& preferredAntipodalAxis = { 0.0f, 0.0f, 0.0f }) {
   GameEngine::Vector3 from = NormalizeOrFallback(current, fallback);
   GameEngine::Vector3 to = NormalizeOrFallback(target, from);
   float t = std::clamp(blend, 0.0f, 1.0f);
   if (t <= 0.0f) {
	  return from;
   }
   if (t >= 1.0f) {
	  return to;
   }

   float angle = std::acos(std::clamp(from.Dot(to), -1.0f, 1.0f));
   return RotateTowardsUnit(from, to, angle * t, preferredAntipodalAxis);
}

/// @brief 単位ベクトルを指定軸まわりに回転する
inline GameEngine::Vector3 RotateAroundAxisUnit(const GameEngine::Vector3& value,
   const GameEngine::Vector3& axis,
   float radians) {
   GameEngine::Vector3 v = NormalizeOrFallback(value, { 0.0f, 0.0f, 1.0f });
   GameEngine::Vector3 n = NormalizeOrFallback(axis, { 0.0f, 1.0f, 0.0f });
   float cs = std::cos(radians);
   float sn = std::sin(radians);
   GameEngine::Vector3 out = v * cs + n.Cross(v) * sn + n * (n.Dot(v) * (1.0f - cs));
   return NormalizeOrFallback(out, v);
}

/// @brief 同一平面上の単位方向同士の符号付き角度を返す
/// @details 180度付近では外積がほぼゼロになって符号を失うため、直前の旋回符号を使う。
inline float SignedAngleAroundAxis(const GameEngine::Vector3& from,
   const GameEngine::Vector3& to,
   const GameEngine::Vector3& axis,
   float antipodalSign) {
   GameEngine::Vector3 safeFrom = NormalizeOrFallback(from, { 0.0f, 0.0f, 1.0f });
   GameEngine::Vector3 safeTo = NormalizeOrFallback(to, safeFrom);
   GameEngine::Vector3 safeAxis = NormalizeOrFallback(axis, { 0.0f, 1.0f, 0.0f });
   float cosine = std::clamp(safeFrom.Dot(safeTo), -1.0f, 1.0f);
   float sine = safeAxis.Dot(safeFrom.Cross(safeTo));
   if (std::abs(sine) <= 1e-5f && cosine < -0.9999f) {
      return antipodalSign < 0.0f ? -GameEngine::MathConstants::kPi : GameEngine::MathConstants::kPi;
   }
   return std::atan2(sine, cosine);
}

/// @brief 0..1 の値を端で滑らかになるS字カーブへ変換する
inline float SmoothStep01(float value) {
   float t = std::clamp(value, 0.0f, 1.0f);
   return t * t * (3.0f - 2.0f * t);
}

/// @brief 2方向のなす角を度で返す
inline float AngleDegrees(const GameEngine::Vector3& from, const GameEngine::Vector3& to) {
   float fromLength = from.Length();
   float toLength = to.Length();
   if (fromLength <= 1e-6f || toLength <= 1e-6f) {
      return 0.0f;
   }
   float dot = from.Dot(to) / (fromLength * toLength);
   return std::acos(std::clamp(dot, -1.0f, 1.0f)) * kRadiansToDegrees;
}

/// @brief ベクトルの全要素が有限値かを返す
inline bool IsFiniteVector(const GameEngine::Vector3& value) {
   return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

/// @brief カメラの直交基底からワールド回転クォータニオンを構築する
/// @details ビュー行列の回転部はワールド回転の転置なので、軸を列へ格納して変換する。
inline GameEngine::Quaternion MakeCameraRotationFromBasis(
   const GameEngine::Vector3& right,
   const GameEngine::Vector3& up,
   const GameEngine::Vector3& forward) {
   GameEngine::Matrix4x4 viewRotation{};
   viewRotation.m[0][0] = right.x;
   viewRotation.m[1][0] = right.y;
   viewRotation.m[2][0] = right.z;
   viewRotation.m[0][1] = up.x;
   viewRotation.m[1][1] = up.y;
   viewRotation.m[2][1] = up.z;
   viewRotation.m[0][2] = forward.x;
   viewRotation.m[1][2] = forward.y;
   viewRotation.m[2][2] = forward.z;
   viewRotation.m[3][3] = 1.0f;
   return GameEngine::MatrixToQuaternion(viewRotation);
}

/// @brief ベクトルを平面へ投影して正規化する（失敗時はフォールバックを返す）
/// @details 重力平面への水平化に使用する。投影結果が極小（up と平行）な場合は
///          fallback の投影を試み、それも失敗したら代替軸を返す。
inline GameEngine::Vector3 ProjectOnPlaneNorm(const GameEngine::Vector3& v,
   const GameEngine::Vector3& up,
   const GameEngine::Vector3& fallback) {
   using namespace GameEngine;

   Vector3 proj = v - up * up.Dot(v);
   float len = proj.Length();
   if (len > 1e-4f) {
	  return proj * (1.0f / len);
   }

   // v がほぼ up と平行 → fallback を投影して使う
   Vector3 fb = fallback - up * up.Dot(fallback);
   float fbLen = fb.Length();
   if (fbLen > 1e-4f) {
	  return fb * (1.0f / fbLen);
   }

   // それも失敗（up と fallback が平行）→ 代替軸を構築して返す
   Vector3 axis = (std::abs(up.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
   Vector3 out = axis - up * up.Dot(axis);
   float outLen = out.Length();
   return outLen > 1e-4f ? out * (1.0f / outLen) : Vector3{ 0.0f, 0.0f, 1.0f };
}


} // namespace App::RearCameraMath
