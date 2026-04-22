#pragma once
#include <algorithm>
#include <cmath>
#include "Vector2.h"

namespace GameEngine {

struct Vector3 {
   float x, y, z;

   static Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
      t = std::clamp(t, 0.0f, 1.0f);
      return start + (end - start) * t;
   }

   static Vector3 Slerp(const Vector3& start, const Vector3& end, float t) {
      t = std::clamp(t, 0.0f, 1.0f);

      // 0ベクトルチェック
      if (start.LengthSquared() < 1e-8f || end.LengthSquared() < 1e-8f) {
         return Lerp(start, end, t);
      }

      Vector3 normalizedStart = start.Normalize();
      Vector3 normalizedEnd = end.Normalize();
      float dot = std::clamp(normalizedStart.Dot(normalizedEnd), -1.0f, 1.0f);

      if (dot > 0.9995f) {
         return Lerp(normalizedStart, normalizedEnd, t).Normalize();
      }

      float theta = std::acos(dot);
      float sinTheta = std::sin(theta);
      if (std::abs(sinTheta) < 1e-5f) {
         return Lerp(normalizedStart, normalizedEnd, t).Normalize();
      }

      return (normalizedStart * std::sin((1.0f - t) * theta)
         + normalizedEnd * std::sin(t * theta))
         / sinTheta;
   }

   Vector3 operator+(const Vector3& vector) const { return { x + vector.x, y + vector.y, z + vector.z }; }
   Vector3 operator-(const Vector3& vector) const { return { x - vector.x, y - vector.y, z - vector.z }; }
   Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
   Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }
   Vector3 operator+=(const Vector3& vector) { x += vector.x; y += vector.y; z += vector.z; return *this; }
   Vector3 operator+=(const float scalar) { x += scalar;	y += scalar; z += scalar; return *this; }
   Vector3 operator-=(const Vector3& vector) { x -= vector.x; y -= vector.y; z -= vector.z; return *this; }
   Vector3 operator-() const { return { -x, -y, -z }; }
   Vector3 operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

   Vector3 Cross(const Vector3& vector) const { return { y * vector.z - z * vector.y, z * vector.x - x * vector.z, x * vector.y - y * vector.x }; }
   float Dot(const Vector3& vector) const { return x * vector.x + y * vector.y + z * vector.z; }
   float Length() const { return std::sqrt(x * x + y * y + z * z); }
   float LengthSquared() const { return x * x + y * y + z * z; }
   Vector3 Normalize() const { 
      float length = Length(); 
      // 0除算チェック：長さが極小の場合は0ベクトルを返す
      if (length < 1e-8f) { 
         return { 0.0f, 0.0f, 0.0f }; 
      }
      return { x / length, y / length, z / length }; 
   }
   Vector3 Project(const Vector3& vector) const { Vector3 normalized = vector.Normalize(); return normalized * Dot(normalized); }
   Vector3 Perpendicular() const { if (x != 0.0f || y != 0.0f) { return { -y,x,0.0f }; } return { 0.0f,-z,y }; }
};

} // namespace GameEngine