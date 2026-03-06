#pragma once
#include "Matrix4x4.h"

namespace GameEngine {

struct Vector4 {
   float x, y, z, w;

   Vector4 operator+(const Vector4& other) const { return { x + other.x, y + other.y, z + other.z, w + other.w }; }
   Vector4 operator-(const Vector4& other) const { return { x - other.x, y - other.y, z - other.z, w - other.w }; }
   Vector4 operator*(const float& scalar) const { return { x * scalar, y * scalar, z * scalar, w * scalar }; }

   Vector4 operator*(const Matrix4x4& mat) const {
	  Vector4 result;
	  result.x = mat.m[0][0] * x + mat.m[0][1] * y + mat.m[0][2] * z + mat.m[0][3] * w;
	  result.y = mat.m[1][0] * x + mat.m[1][1] * y + mat.m[1][2] * z + mat.m[1][3] * w;
	  result.z = mat.m[2][0] * x + mat.m[2][1] * y + mat.m[2][2] * z + mat.m[2][3] * w;
	  result.w = mat.m[3][0] * x + mat.m[3][1] * y + mat.m[3][2] * z + mat.m[3][3] * w;
	  return result;
   }

   Vector4& operator/=(float scalar) {
	  x /= scalar;
	  y /= scalar;
	  z /= scalar;
	  w /= scalar;
	  return *this;
   }
};

} // namespace GameEngine