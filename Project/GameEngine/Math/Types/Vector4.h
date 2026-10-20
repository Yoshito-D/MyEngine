#pragma once

#include "GameEngine/Math/Types/Matrix4x4.h"

namespace GameEngine {

/// @brief 4つの浮動小数点成分を持つベクトル。
/// @details 3次元の同次座標やRGBA色など、4成分をまとめて扱う用途に使用する。
struct Vector4 {
   float x; //!< 第1成分
   float y; //!< 第2成分
   float z; //!< 第3成分
   float w; //!< 第4成分。同次座標として使う場合は位置・方向の区別や透視除算に使用する

   /// @brief 各成分を加算する。
   /// @param other 加算するベクトル。
   /// @return このベクトルとotherの成分ごとの和。
   Vector4 operator+(const Vector4& other) const { return {x + other.x, y + other.y, z + other.z, w + other.w}; }

   /// @brief 各成分を減算する。
   /// @param other 減算するベクトル。
   /// @return このベクトルからotherを成分ごとに引いた差。
   Vector4 operator-(const Vector4& other) const { return {x - other.x, y - other.y, z - other.z, w - other.w}; }

   /// @brief 全成分へ同じスカラーを乗算する。
   /// @param scalar 乗算する係数。
   /// @return スケーリング後のベクトル。
   Vector4 operator*(const float& scalar) const { return {x * scalar, y * scalar, z * scalar, w * scalar}; }

   /// @brief 行列の各行との内積によって4次元ベクトルを変換する。
   /// @param mat 適用する4x4行列。
   /// @return result[i] = mat.m[i][0] * x + ... + mat.m[i][3] * wとして求めたベクトル。
   /// @note 演算子の記法はvector * matrixだが、要素計算は行列の各行を左側に置く規約で行う。
   Vector4 operator*(const Matrix4x4& mat) const;

   /// @brief 全成分を同じスカラーで除算し、このベクトルを更新する。
   /// @param scalar 除数。
   /// @return 更新後のこのベクトルへの参照。
   /// @pre scalarは0でないこと。
   Vector4& operator/=(float scalar);
};

} // namespace GameEngine
