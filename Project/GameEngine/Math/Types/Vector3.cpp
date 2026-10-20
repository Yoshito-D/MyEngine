#include "GameEngine/pch.h"
#include "GameEngine/Math/Types/Vector3.h"

namespace GameEngine {

Vector3 Vector3::Lerp(const Vector3& start, const Vector3& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   return start + (end - start) * t;
}

Vector3 Vector3::Slerp(const Vector3& start, const Vector3& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);

   // 球面補間には方向が必要なため、極小ベクトルを含む場合は元の長さを保つ線形補間へ退避する。
   if (start.LengthSquared() < 1e-8f || end.LengthSquared() < 1e-8f) {
      return Lerp(start, end, t);
   }

   Vector3 normalizedStart = start.Normalize();
   Vector3 normalizedEnd = end.Normalize();
   float dot = std::clamp(normalizedStart.Dot(normalizedEnd), -1.0f, 1.0f);

   // 方向が近いとsin(theta)による除算が不安定になるため、正規化線形補間で連続性を保つ。
   if (dot > 0.9995f) {
      return Lerp(normalizedStart, normalizedEnd, t).Normalize();
   }

   float theta = std::acos(dot);
   float sinTheta = std::sin(theta);

   // 主に反対向きで回転面を決められない場合を処理し、極小値による除算を避ける。
   if (std::abs(sinTheta) < 1e-5f) {
      return Lerp(normalizedStart, normalizedEnd, t).Normalize();
   }

   return (normalizedStart * std::sin((1.0f - t) * theta) + normalizedEnd * std::sin(t * theta)) / sinTheta;
}

Vector3 Vector3::operator+=(const Vector3& vector) {
   x += vector.x;
   y += vector.y;
   z += vector.z;
   return *this;
}

Vector3 Vector3::operator+=(const float scalar) {
   x += scalar;
   y += scalar;
   z += scalar;
   return *this;
}

Vector3 Vector3::operator-=(const Vector3& vector) {
   x -= vector.x;
   y -= vector.y;
   z -= vector.z;
   return *this;
}

Vector3 Vector3::operator/=(float scalar) {
   x /= scalar;
   y /= scalar;
   z /= scalar;
   return *this;
}

Vector3 Vector3::Cross(const Vector3& vector) const {
   return {y * vector.z - z * vector.y, z * vector.x - x * vector.z, x * vector.y - y * vector.x};
}

Vector3 Vector3::Normalize() const {
   float length = Length();

   // 0除算チェック：長さが極小の場合は0ベクトルを返す
   if (length < 1e-8f) {
      return {0.0f, 0.0f, 0.0f};
   }

   return {x / length, y / length, z / length};
}

Vector3 Vector3::Project(const Vector3& vector) const {
   Vector3 normalized = vector.Normalize();
   return normalized * Dot(normalized);
}

Vector3 Vector3::Perpendicular() const {
   if (x != 0.0f || y != 0.0f) {
      return {-y, x, 0.0f};
   }

   return {0.0f, -z, y};
}

Quaternion Vector3::ToQuaternion() const {
   float halfX = x * 0.5f;
   float halfY = y * 0.5f;
   float halfZ = z * 0.5f;

   float cosX = std::cos(halfX);
   float cosY = std::cos(halfY);
   float cosZ = std::cos(halfZ);
   float sinX = std::sin(halfX);
   float sinY = std::sin(halfY);
   float sinZ = std::sin(halfZ);

   return Quaternion{sinX * cosY * cosZ - cosX * sinY * sinZ, cosX * sinY * cosZ + sinX * cosY * sinZ,
      cosX * cosY * sinZ - sinX * sinY * cosZ, cosX * cosY * cosZ + sinX * sinY * sinZ};
}

} // namespace GameEngine
