#include "pch.h"
#include "QuaternionOperations.h"

namespace GameEngine {

Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle) {
   Vector3 n = axis.Normalize();
   float halfAngle = angle * 0.5f;
   float s = std::sin(halfAngle);
   Quaternion result = {
	   n.x * s,
	   n.y * s,
	   n.z * s,
	   std::cos(halfAngle)
   };
   return result;
}

Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion) {
   Quaternion p = { vector.x, vector.y, vector.z, 0.0f };
   Quaternion qConjugate = quaternion.Conjugate();
   Quaternion rotatedP = quaternion * p * qConjugate;
   Vector3 result = { rotatedP.x, rotatedP.y, rotatedP.z };
   return result;
}

Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion) {
   Matrix4x4 result = {
	   std::powf(quaternion.w,2) + std::powf(quaternion.x,2) - std::powf(quaternion.y,2) - std::powf(quaternion.z,2),
	   2.0f * (quaternion.x * quaternion.y + quaternion.w * quaternion.z),
	   2.0f * (quaternion.x * quaternion.z - quaternion.w * quaternion.y),
	   0.0f,
	   2.0f * (quaternion.x * quaternion.y - quaternion.w * quaternion.z),
	   std::powf(quaternion.w,2) - std::powf(quaternion.x,2) + std::powf(quaternion.y,2) - std::powf(quaternion.z,2),
	   2.0f * (quaternion.y * quaternion.z + quaternion.w * quaternion.x),
	   0.0f,
	   2.0f * (quaternion.x * quaternion.z + quaternion.w * quaternion.y),
	   2.0f * (quaternion.y * quaternion.z - quaternion.w * quaternion.x),
	   std::powf(quaternion.w,2) - std::powf(quaternion.x,2) - std::powf(quaternion.y,2) + std::powf(quaternion.z,2),
	   0.0f,
	   0.0f, 0.0f, 0.0f, 1.0f
   };
   return result;
}

Quaternion MatrixToQuaternion(const Matrix4x4& m) {
   Quaternion q;
   float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];

   if (trace > 0.0f) {
	  float s = std::sqrt(trace + 1.0f) * 2.0f;
	  q.w = 0.25f * s;
	  q.x = (m.m[2][1] - m.m[1][2]) / s;
	  q.y = (m.m[0][2] - m.m[2][0]) / s;
	  q.z = (m.m[1][0] - m.m[0][1]) / s;
   } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
	  float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f;
	  q.w = (m.m[2][1] - m.m[1][2]) / s;
	  q.x = 0.25f * s;
	  q.y = (m.m[0][1] + m.m[1][0]) / s;
	  q.z = (m.m[0][2] + m.m[2][0]) / s;
   } else if (m.m[1][1] > m.m[2][2]) {
	  float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f;
	  q.w = (m.m[0][2] - m.m[2][0]) / s;
	  q.x = (m.m[0][1] + m.m[1][0]) / s;
	  q.y = 0.25f * s;
	  q.z = (m.m[1][2] + m.m[2][1]) / s;
   } else {
	  float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f;
	  q.w = (m.m[1][0] - m.m[0][1]) / s;
	  q.x = (m.m[0][2] + m.m[2][0]) / s;
	  q.y = (m.m[1][2] + m.m[2][1]) / s;
	  q.z = 0.25f * s;
   }

   return q.Normalize();
}

Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) {
   t = std::clamp(t, 0.0f, 1.0f);

   float dot = q0.Dot(q1);

   Quaternion Q0 = q0;
   Quaternion Q1 = q1;

   // --- 1. dot を [-1, 1] に必ずクランプ ---
   dot = std::clamp(dot, -1.0f, 1.0f);

   // --- 2. 反転補正（最短経路） ---
   if (dot < 0.0f) {
	  Q1 = -Q1;
	  dot = -dot;
   }

   // --- 3. 非常に近い場合は Lerp にフォールバック ---
   const float epsilon = 1e-6f;
   if (1.0f - dot < epsilon) {
	  // Lerp
	  Quaternion result = Q0 * (1.0f - t) + Q1 * t;
	  return result.Normalize();
   }

   // --- 4. 通常の Slerp ---
   float theta = std::acos(dot);
   float sinTheta = std::sin(theta);

   float s0 = std::sin((1 - t) * theta) / sinTheta;
   float s1 = std::sin(t * theta) / sinTheta;

   Quaternion result = Q0 * s0 + Q1 * s1;
   return result.Normalize();
}

Quaternion Vector3ToQuaternion(const Vector3& eulerAngles) {
   float cy = std::cos(eulerAngles.y * 0.5f);
   float sy = std::sin(eulerAngles.y * 0.5f);
   float cp = std::cos(eulerAngles.x * 0.5f);
   float sp = std::sin(eulerAngles.x * 0.5f);
   float cr = std::cos(eulerAngles.z * 0.5f);
   float sr = std::sin(eulerAngles.z * 0.5f);
   Quaternion q;
   q.w = cr * cp * cy + sr * sp * sy;
   q.x = sr * cp * cy - cr * sp * sy;
   q.y = cr * sp * cy + sr * cp * sy;
   return q;
}

Quaternion LookRotation(const Vector3& forward, const Vector3& up) {
   Vector3 f = forward;
   if (f.LengthSquared() < 1e-6f)
	  f = Vector3(0, 0, 1);
   else
	  f.Normalize();

   Vector3 u = up;
   if (u.LengthSquared() < 1e-6f)
	  u = Vector3(0, 1, 0);
   else
	  u.Normalize();

   // forward と up が平行なら別の up ベクトルに差し替え
   if (fabs(f.Dot(u)) > 0.999f) {
	  if (fabs(f.y) < 0.99f)
		 u = Vector3(0, 1, 0);
	  else
		 u = Vector3(1, 0, 0);
   }

   // 左手系: right = up × forward
   Vector3 r = u.Cross(f);
   if (r.LengthSquared() < 1e-6f)
	  r = Vector3(1, 0, 0);
   else
	  r.Normalize();

   // 直交 up 再計算（左手系） up = forward × right
   u = f.Cross(r);

   Matrix4x4 m = {
	   r.x, r.y, r.z, 0.0f,
	   u.x, u.y, u.z, 0.0f,
	   f.x, f.y, f.z, 0.0f,
	   0.0f,0.0f,0.0f, 1.0f
   };
   return MatrixToQuaternion(m);
}

Quaternion LookRotation(const Vector3& forward) {
   return LookRotation(forward, Vector3(0.0f, 1.0f, 0.0f));
}

} // namespace GameEngine
