#include "GameEngine/Math/Types/Quaternion.h"
#include "GameEngine/Math/Types/Vector3.h"

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

namespace GameEngine {

Quaternion Quaternion::Lerp(const Quaternion& start, const Quaternion& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   Quaternion target = end;

   // qと-qは同じ回転なので、内積が負なら終点を反転し、4次元球面上の短い側を補間する。
   if (start.Dot(target) < 0.0f) {
      target = -target;
   }

   return (start * (1.0f - t) + target * t).Normalize();
}

Quaternion Quaternion::Slerp(const Quaternion& start, const Quaternion& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);

   Quaternion q0 = start;
   Quaternion q1 = end;
   float dot = std::clamp(q0.Dot(q1), -1.0f, 1.0f);

   // qと-qが同じ姿勢を表す性質を利用し、常に短い回転経路を選ぶ。
   if (dot < 0.0f) {
      q1 = -q1;
      dot = -dot;
   }

   const float epsilon = 1e-6f;

   // 角度が極小だとsin(theta)による除算が不安定になるため、正規化線形補間へ切り替える。
   if (1.0f - dot < epsilon) {
      return Lerp(q0, q1, t);
   }

   float theta = std::acos(dot);
   float sinTheta = std::sin(theta);
   float s0 = std::sin((1.0f - t) * theta) / sinTheta;
   float s1 = std::sin(t * theta) / sinTheta;
   return (q0 * s0 + q1 * s1).Normalize();
}

Quaternion Quaternion::operator*(const Quaternion& q) const {
   return Quaternion{w * q.x + x * q.w + y * q.z - z * q.y, w * q.y - x * q.z + y * q.w + z * q.x,
      w * q.z + x * q.y - y * q.x + z * q.w, w * q.w - x * q.x - y * q.y - z * q.z};
}

Quaternion Quaternion::Normalize() const {
   float norm = Norm();

   // 0除算チェック：ノルムが極小の場合はIdentityを返す
   if (norm < 1e-8f) {
      return Identity();
   }

   return Quaternion{x / norm, y / norm, z / norm, w / norm};
}

Quaternion Quaternion::Inverse() const {
   Quaternion conj = Conjugate();
   float normSq = x * x + y * y + z * z + w * w;

   // 0除算チェック：ノルムの2乗が極小の場合はIdentityを返す
   if (normSq < 1e-8f) {
      return Identity();
   }

   return Quaternion{conj.x / normSq, conj.y / normSq, conj.z / normSq, conj.w / normSq};
}

} // namespace GameEngine
