#include "pch.h"
#include "Collision.h"

namespace GameEngine {
bool Collision::IsCollision(const Collider::Sphere& sphere1, const Collider::Sphere& sphere2) {
   float distance = (sphere1.center - sphere2.center).Length();

   if (distance <= sphere1.radius + sphere2.radius) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Sphere& sphere, const Collider::Plane& plane) {
   float distance = std::abs(plane.normal.Dot(sphere.center) - plane.distance);

   if (distance <= sphere.radius) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Segment& segment, const Collider::Plane& plane) {
   float dot = plane.normal.Dot(segment.diff);

   if (dot == 0.0f) {
	  return false;
   }

   float t = (plane.distance - segment.origin.Dot(plane.normal)) / dot;

   if (t >= 0.0f && t <= 1.0f) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Ray& ray, const Collider::Plane& plane) {
   float dot = plane.normal.Dot(ray.diff);

   if (dot == 0.0f) {
	  return false;
   }

   float t = (plane.distance - ray.origin.Dot(plane.normal)) / dot;

   if (t >= 0.0f) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Line& line, const Collider::Plane& plane) {
   float dot = plane.normal.Dot(line.diff);

   if (dot == 0.0f) {
	  return false;
   }

   return true;
}

bool Collision::IsCollision(const Collider::Triangle& triangle, const Collider::Segment& segment) {
   Vector3 normal = (triangle.vertices[1] - triangle.vertices[0]).Cross(triangle.vertices[2] - triangle.vertices[0]);
   normal.Normalize();

   float dot = normal.Dot(segment.diff);

   if (dot == 0.0f) {
	  return false;
   }

   float t = (triangle.vertices[0] - segment.origin).Dot(normal) / segment.diff.Dot(normal);
   if (t < 0.0f || t > 1.0f) {
	  return false;
   }

   Vector3 p = segment.origin + segment.diff * t;

   Vector3 cross01 = (triangle.vertices[0] - triangle.vertices[1]).Cross(triangle.vertices[1] - p);
   Vector3 cross12 = (triangle.vertices[1] - triangle.vertices[2]).Cross(triangle.vertices[2] - p);
   Vector3 cross20 = (triangle.vertices[2] - triangle.vertices[0]).Cross(triangle.vertices[0] - p);

   if (cross01.Dot(normal) >= 0.0f && cross12.Dot(normal) >= 0.0f && cross20.Dot(normal) >= 0.0f) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Triangle& triangle, const Collider::Ray& ray) {
   Vector3 normal = (triangle.vertices[1] - triangle.vertices[0]).Cross(triangle.vertices[2] - triangle.vertices[0]);
   normal.Normalize();

   float dot = normal.Dot(ray.diff);

   if (dot == 0.0f) {
	  return false;
   }

   float t = (triangle.vertices[0] - ray.origin).Dot(normal) / ray.diff.Dot(normal);
   if (t < 0.0f) {
	  return false;
   }

   Vector3 p = ray.origin + ray.diff * t;
   Vector3 cross01 = (triangle.vertices[0] - triangle.vertices[1]).Cross(triangle.vertices[1] - p);
   Vector3 cross12 = (triangle.vertices[1] - triangle.vertices[2]).Cross(triangle.vertices[2] - p);
   Vector3 cross20 = (triangle.vertices[2] - triangle.vertices[0]).Cross(triangle.vertices[0] - p);

   if (cross01.Dot(normal) >= 0.0f && cross12.Dot(normal) >= 0.0f && cross20.Dot(normal) >= 0.0f) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::Triangle& triangle, const Collider::Line& line) {
   Vector3 normal = (triangle.vertices[1] - triangle.vertices[0]).Cross(triangle.vertices[2] - triangle.vertices[0]);
   normal.Normalize();

   float dot = normal.Dot(line.diff);

   if (dot == 0.0f) {
	  return false;
   }

   float t = (triangle.vertices[0] - line.origin).Dot(normal) / line.diff.Dot(normal);
   Vector3 p = line.origin + line.diff * t;
   Vector3 cross01 = (triangle.vertices[0] - triangle.vertices[1]).Cross(triangle.vertices[1] - p);
   Vector3 cross12 = (triangle.vertices[1] - triangle.vertices[2]).Cross(triangle.vertices[2] - p);
   Vector3 cross20 = (triangle.vertices[2] - triangle.vertices[0]).Cross(triangle.vertices[0] - p);

   if (cross01.Dot(normal) >= 0.0f && cross12.Dot(normal) >= 0.0f && cross20.Dot(normal) >= 0.0f) {
	  return true;
   }
   return false;
}

bool Collision::IsCollision(const Collider::AABB& aabb1, const Collider::AABB& aabb2) {
   if ((aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) &&
	  (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) &&
	  (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z)) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::AABB& aabb, const Collider::Sphere& sphere) {
   Vector3 closestPoint{
	   std::clamp(sphere.center.x,aabb.min.x,aabb.max.x),
	   std::clamp(sphere.center.y,aabb.min.y,aabb.max.y),
	   std::clamp(sphere.center.z,aabb.min.z,aabb.max.z)
   };

   float distance = (closestPoint - sphere.center).Length();

   if (distance <= sphere.radius) {
	  return true;
   }

   return false;
}

bool Collision::IsCollision(const Collider::AABB& aabb, const Collider::Segment& segment) {
   float tMin = -INFINITY;
   float tMax = INFINITY;

   // X軸
   if (segment.diff.x != 0.0f) {
	  float t1 = (aabb.min.x - segment.origin.x) / segment.diff.x;
	  float t2 = (aabb.max.x - segment.origin.x) / segment.diff.x;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (segment.origin.x < aabb.min.x || segment.origin.x > aabb.max.x) {
	  return false;
   }

   // Y軸
   if (segment.diff.y != 0.0f) {
	  float t1 = (aabb.min.y - segment.origin.y) / segment.diff.y;
	  float t2 = (aabb.max.y - segment.origin.y) / segment.diff.y;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (segment.origin.y < aabb.min.y || segment.origin.y > aabb.max.y) {
	  return false;
   }

   // Z軸
   if (segment.diff.z != 0.0f) {
	  float t1 = (aabb.min.z - segment.origin.z) / segment.diff.z;
	  float t2 = (aabb.max.z - segment.origin.z) / segment.diff.z;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (segment.origin.z < aabb.min.z || segment.origin.z > aabb.max.z) {
	  return false;
   }

   if (tMin > tMax) {
	  return false;
   }

   return tMax >= 0.0f && tMin <= 1.0f;
}

bool Collision::IsCollision(const Collider::AABB& aabb, const Collider::Ray& ray) {
   float tMin = -INFINITY;
   float tMax = INFINITY;
   // X軸

   if (ray.diff.x != 0.0f) {
	  float t1 = (aabb.min.x - ray.origin.x) / ray.diff.x;
	  float t2 = (aabb.max.x - ray.origin.x) / ray.diff.x;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
	  return false;
   }

   // Y軸
   if (ray.diff.y != 0.0f) {
	  float t1 = (aabb.min.y - ray.origin.y) / ray.diff.y;
	  float t2 = (aabb.max.y - ray.origin.y) / ray.diff.y;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
	  return false;
   }

   // Z軸
   if (ray.diff.z != 0.0f) {
	  float t1 = (aabb.min.z - ray.origin.z) / ray.diff.z;
	  float t2 = (aabb.max.z - ray.origin.z) / ray.diff.z;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
	  return false;
   }

   if (tMin > tMax) {
	  return false;
   }

   return tMax >= 0.0f;
}

bool Collision::IsCollision(const Collider::AABB& aabb, const Collider::Line& line) {
   float tMin = -INFINITY;
   float tMax = INFINITY;

   // X軸
   if (line.diff.x != 0.0f) {
	  float t1 = (aabb.min.x - line.origin.x) / line.diff.x;
	  float t2 = (aabb.max.x - line.origin.x) / line.diff.x;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (line.origin.x < aabb.min.x || line.origin.x > aabb.max.x) {
	  return false;
   }

   // Y軸
   if (line.diff.y != 0.0f) {
	  float t1 = (aabb.min.y - line.origin.y) / line.diff.y;
	  float t2 = (aabb.max.y - line.origin.y) / line.diff.y;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (line.origin.y < aabb.min.y || line.origin.y > aabb.max.y) {
	  return false;
   }

   // Z軸
   if (line.diff.z != 0.0f) {
	  float t1 = (aabb.min.z - line.origin.z) / line.diff.z;
	  float t2 = (aabb.max.z - line.origin.z) / line.diff.z;
	  tMin = std::max(tMin, std::min(t1, t2));
	  tMax = std::min(tMax, std::max(t1, t2));
   } else if (line.origin.z < aabb.min.z || line.origin.z > aabb.max.z) {
	  return false;
   }

   if (tMin > tMax) {
	  return false;
   }

   return true;
}

bool Collision::IsCollision(const Collider::Capsule& capsule, const Collider::Plane& plane) {
   Vector3 a = capsule.segment.origin;
   Vector3 b = a + capsule.segment.diff;

   float da = plane.SignedDistance(a);
   float db = plane.SignedDistance(b);

   return (da * db <= 0.0f) || (fabsf(da) < capsule.radius) || (fabsf(db) < capsule.radius);
}

bool Collision::IsCollision(const Collider::Ray& ray, const Collider::Sphere& sphere) {
   Vector3 m = ray.origin - sphere.center;

   float b = m.Dot(ray.diff);                // (m · diff)
   float c = m.Dot(m) - sphere.radius * sphere.radius;

   // レイが球の外側にあり、球から遠ざかる方向なら当たらない
   if (c > 0.0f && b > 0.0f) {
	  return false;
   }

   float diffLen2 = ray.diff.Dot(ray.diff);  // |diff|^2（正規化不要）
   float disc = b * b - diffLen2 * c;        // 判別式

   return disc >= 0.0f; // disc < 0 → 解なし → 当たらない
}
}
