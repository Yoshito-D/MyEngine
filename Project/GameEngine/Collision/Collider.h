#pragma once
#include "Utility/VectorMath.h"

namespace GameEngine {
namespace Collider {
struct Sphere {
   Vector3 center; //!< 中心点
   float radius; //!< 半径
};

struct Line {
   Vector3 origin; //!< 始点
   Vector3 diff; //!< 終点への差分ベクトル
};

struct Plane {
   Vector3 normal; //!< 法線
   float distance; //!< 距離

   float SignedDistance(const Vector3& point) const {
	  return normal.Dot(point) - distance;
   }
};

struct Ray {
   Vector3 origin; //!< 始点
   Vector3 diff; //!< 終点への差分ベクトル
};

struct Segment {
   Vector3 origin; //!< 始点
   Vector3 diff; //!< 終点への差分ベクトル
};

struct Triangle {
   Vector3 vertices[3]; //!< 頂点
};

struct AABB {
   Vector3 min; //!< 最小点
   Vector3 max; //!< 最大点
};

struct Spring {
   Vector3 anchor; //!< アンカー
   float naturalLength; //!< 自然長
   float stiffness; //!< 剛性
   float dampingCoefficient; //!< 減衰係数
};

struct Ball {
   Vector3 position; //!< 位置
   Vector3 velocity; //!< 速度
   Vector3 acceleration; //!< 加速度
   float mass; //!< 質量
   float radius; //!< 半径
   unsigned int color; //!< 色
};

struct Pendulum {
   Vector3 anchor; //!< アンカー
   float length; //!< 長さ	
   float angle; //!< 角度
   float angularVelocity; //!< 角速度
   float angularAcceleration; //!< 角加速度
};

struct ConicalPendulum {
   Vector3 anchor; //!< アンカー
   float length; //!< 長さ	
   float halfApexAngle; //!< 半頂角
   float angle; //!< 角度
   float angularVelocity; //!< 角速度
};

struct Capsule {
   Segment segment; //!< 線分
   float radius; //!< 半径
};
};
}