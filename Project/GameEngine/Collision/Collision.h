#pragma once
#include "Collider.h"

namespace GameEngine {

/// @brief 衝突判定関数群
namespace Collision {
/// @brief 球と球の衝突判定
/// @param sphere1 球1
/// @param sphere2 球2
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Sphere& sphere1, const Collider::Sphere& sphere2);

/// @brief 球と平面の衝突判定
/// @param sphere 球
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Sphere& sphere, const Collider::Plane& plane);

/// @brief 線分と平面の衝突判定
/// @param segment 線分
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Segment& segment, const Collider::Plane& plane);

/// @brief 線分と平面の衝突判定
/// @param segment 線分
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Segment& segment, const Collider::Plane& plane);

/// @brief レイと平面の衝突判定
/// @param ray レイ
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Ray& ray, const Collider::Plane& plane);

/// @brief 直線と平面の衝突判定
/// @param line 直線
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Line& line, const Collider::Plane& plane);

/// @brief 三角形と線分の衝突判定
/// @param triangle 三角形
/// @param segment 線分
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Triangle& triangle, const Collider::Segment& segment);

/// @brief 三角形とレイの衝突判定
/// @param triangle 三角形
/// @param ray レイ
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Triangle& triangle, const Collider::Ray& ray);

/// @brief 三角形と直線の衝突判定
/// @param triangle 三角形
/// @param line 直線
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Triangle& triangle, const Collider::Line& line);

/// @brief AABB同士の衝突判定
/// @param aabb1 AABB1
/// @param aabb2 AABB2
/// @return 衝突しているかどうか
bool IsCollision(const Collider::AABB& aabb1, const Collider::AABB& aabb2);

/// @brief AABBと球の衝突判定
/// @param aabb AABB
/// @param sphere 
/// @return 衝突しているかどうか
bool IsCollision(const Collider::AABB& aabb, const Collider::Sphere& sphere);

/// @brief AABBと線分の衝突判定
/// @param aabb AABB
/// @param segment 線分
/// @return 衝突しているかどうか
bool IsCollision(const Collider::AABB& aabb, const Collider::Segment& segment);

/// @brief AABBとレイの衝突判定
/// @param aabb AABB
/// @param ray レイ
/// @return 衝突しているかどうか
bool IsCollision(const Collider::AABB& aabb, const Collider::Ray& ray);

/// @brief AABBと直線の衝突判定
/// @param aabb AABB
/// @param line 直線
/// @return 衝突しているかどうか
bool IsCollision(const Collider::AABB& aabb, const Collider::Line& line);

/// @brief カプセルと平面の衝突判定
/// @param capsule カプセル
/// @param plane 平面
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Capsule& capsule, const Collider::Plane& plane);

/// @brief レイと球の衝突判定
/// @param ray レイ
/// @param sphere 球
/// @return 衝突しているかどうか
bool IsCollision(const Collider::Ray& ray, const Collider::Sphere& sphere);
}
}