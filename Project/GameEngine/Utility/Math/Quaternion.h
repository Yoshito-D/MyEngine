#pragma once
#include <cmath>

namespace GameEngine {

struct Quaternion {
   float x;
   float y;
   float z;
   float w;

   static Quaternion Identity() {
	  return Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
   }

   Quaternion operator+(const Quaternion& q) const {
	  return Quaternion{ x + q.x, y + q.y, z + q.z, w + q.w };
   }

   Quaternion operator-() const {
	  return Quaternion{ -x, -y, -z, -w };
   }

   Quaternion operator*(const Quaternion& q) const {
	  return Quaternion{
		  w * q.x + x * q.w + y * q.z - z * q.y,
		  w * q.y - x * q.z + y * q.w + z * q.x,
		  w * q.z + x * q.y - y * q.x + z * q.w,
		  w * q.w - x * q.x - y * q.y - z * q.z
	  };
   }

   Quaternion operator*(float t)const {
	  return Quaternion{ x * t, y * t, z * t, w * t };
   }

   Quaternion Conjugate() const {
	  return Quaternion(-x, -y, -z, w);
   }

   float Norm() const {
	  return std::sqrt(x * x + y * y + z * z + w * w);
   }

   Quaternion Normalize() const {
	  float norm = Norm();
	  return Quaternion{ x / norm, y / norm, z / norm, w / norm };
   }

   Quaternion Inverse() const {
	  Quaternion conj = Conjugate();
	  float normSq = x * x + y * y + z * z + w * w;
	  return Quaternion{ conj.x / normSq, conj.y / normSq, conj.z / normSq, conj.w / normSq };
   }

   float Dot(const Quaternion& q) const {
	  return x * q.x + y * q.y + z * q.z + w * q.w;
   }
};

} // namespace GameEngine