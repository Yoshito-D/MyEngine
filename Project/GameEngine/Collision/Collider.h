#pragma once

#include "GameEngine/Math/VectorMath.h"

namespace GameEngine {

/// @brief 衝突判定で使う幾何形状。
namespace Collider {

/// @brief 中心点と半径で表す3次元の球。
/// @note 衝突判定へ渡す場合、radiusには0以上の値を指定する。
struct Sphere {
   Vector3 center; //!< 中心点
   float radius;   //!< 半径
};

/// @brief 無限直線を原点と方向ベクトルで表す。
/// @details 直線上の点は origin + diff * t（tは任意の実数）で求める。
/// @note diffの長さは任意だが、交差計算に使用する場合は0ベクトルにしない。
struct Line {
   Vector3 origin; //!< 直線上の基準点
   Vector3 diff;   //!< パラメータを進める方向ベクトル
};

/// @brief 法線形式 normal.Dot(point) = distance で表す平面。
/// @note SignedDistanceの戻り値をワールド空間の距離として扱う場合、normalは単位ベクトルにする。
struct Plane {
   Vector3 normal; //!< 法線
   float distance; //!< 平面方程式の定数項。normalが単位ベクトルなら原点からの符号付き距離

   /// @brief 点を平面方程式へ代入し、平面のどちら側にあるかを符号付きで求める。
   /// @param point 平面と同じ座標系で表した判定対象の点。
   /// @return normal.Dot(point) - distance。0なら平面上、正負は法線に対する側を示す。
   /// @note normalが単位ベクトルでない場合、戻り値は法線の長さに比例し、幾何学的な距離にはならない。
   float SignedDistance(const Vector3& point) const { return normal.Dot(point) - distance; }
};

/// @brief 始点から一方向へ無限に延びるレイ。
/// @details レイ上の点は origin + diff * t（t >= 0）で求める。
/// @note diffの長さは任意だが、交差計算に使用する場合は0ベクトルにしない。
struct Ray {
   Vector3 origin; //!< 始点
   Vector3 diff;   //!< 始点から進む方向ベクトル
};

/// @brief 始点と終点間の有限な線分。
/// @details 線分上の点は origin + diff * t（0 <= t <= 1）で求める。
struct Segment {
   Vector3 origin; //!< 始点
   Vector3 diff;   //!< 始点から終点への差分ベクトル
};

/// @brief 3頂点で表す三角形。
/// @note 頂点の並び順が表面法線の向きを決める。面として扱う場合は3頂点を同一直線上に置かない。
struct Triangle {
   Vector3 vertices[3]; //!< 頂点。添字順が面の向きを定義する
};

/// @brief 各座標軸に平行な境界を持つ直方体。
/// @note 有効な範囲として扱うには、各成分でmin <= maxを満たす必要がある。
struct AABB {
   Vector3 min; //!< 最小点
   Vector3 max; //!< 最大点
};

/// @brief 中心線分と一定半径で表すカプセル形状。
/// @note radiusには0以上の値を指定する。segment.diffが0の場合は球と同じ形状になる。
struct Capsule {
   Segment segment; //!< カプセル両端の球中心を結ぶ線分
   float radius;    //!< 中心線分から表面までの半径
};
};
}
