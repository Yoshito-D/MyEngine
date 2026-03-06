#include "pch.h"
#include "SplineUtils.h"
#include <algorithm>
#include <cassert>

namespace GameEngine {

Vector3 CatmullRomInterpolation(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
   const float s = 0.5f;

   float t2 = t * t;
   float t3 = t2 * t;

   Vector3 e3 = -p0 + p1 * 3.0f - p2 * 3.0f + p3;
   Vector3 e2 = p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3;
   Vector3 e1 = -p0 + p2;
   Vector3 e0 = p1 * 2.0f;

   return (e3 * t3 + e2 * t2 + e1 * t + e0) * s;
}

Vector3 CatmullRomPosition(const std::vector<Vector3>& controlPoints, float t) {
   assert(controlPoints.size() >= 4 && "制御点は4点以上必要です");

   size_t division = controlPoints.size() - 1;
   float areaWidth = 1.0f / division;

   size_t index = static_cast<size_t>(t / areaWidth);
   index = std::min(index, division - 1);

   float t_2 = (t - areaWidth * static_cast<float>(index)) / areaWidth;
   t_2 = std::clamp(t_2, 0.0f, 1.0f);

   size_t index0 = index - 1;
   size_t index1 = index;
   size_t index2 = index + 1;
   size_t index3 = index + 2;

   if (index == 0) {
	  index0 = index1;
   }

   if (index3 >= controlPoints.size()) {
	  index3 = index2;
   }

   const Vector3& p0 = controlPoints[index0];
   const Vector3& p1 = controlPoints[index1];
   const Vector3& p2 = controlPoints[index2];
   const Vector3& p3 = controlPoints[index3];

   return CatmullRomInterpolation(p0, p1, p2, p3, t_2);
}

} // namespace GameEngine
