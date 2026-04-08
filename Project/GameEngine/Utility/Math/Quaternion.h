#pragma once
#pragma once
#include <algorithm>
#include <cmath>

namespace GameEngine {

struct Quaternion {
   float x;
   float y;
   float z;
   float w;

   static Quaternion Lerp(const Quaternion& start, const Quaternion& end, float t) {
	  t = std::clamp(t, 0.0f, 1.0f);
	  Quaternion target = end;
	  if (start.Dot(target) < 0.0f) {
		 target = -target;
	  }
	  return (start * (1.0f - t) + target * t).Normalize();
   }

   static Quaternion Slerp(const Quaternion& start, const Quaternion& end, float t) {
	  t = std::clamp(t, 0.0f, 1.0f);

	  Quaternion q0 = start;
	  Quaternion q1 = end;
	  float dot = std::clamp(q0.Dot(q1), -1.0f, 1.0f);

	  if (dot < 0.0f) {
		 q1 = -q1;
		 dot = -dot;
	  }

	  const float epsilon = 1e-6f;
	  if (1.0f - dot < epsilon) {
		 return Lerp(q0, q1, t);
	  }

	  float theta = std::acos(dot);
	  float sinTheta = std::sin(theta);
	  float s0 = std::sin((1.0f - t) * theta) / sinTheta;
	  float s1 = std::sin(t * theta) / sinTheta;
	  return (q0 * s0 + q1 * s1).Normalize();
   }

   static Quaternion Identity() {
	  return Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
   }

   Quaternion operator+(const Quaternion& q) const {
	  return Quaternion{ x + q.x, y + q.y, z + q.z, w + q.w };
   }

   Quaternion operator-(const Quaternion& q) const {
	  return Quaternion{ x - q.x, y - q.y, z - q.z, w - q.w };
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