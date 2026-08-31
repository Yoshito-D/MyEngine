#pragma once
#include "Matrix4x4.h"

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
   Vector4 operator+(const Vector4& other) const { return { x + other.x, y + other.y, z + other.z, w + other.w }; }

   /// @brief 各成分を減算する。
   /// @param other 減算するベクトル。
   /// @return このベクトルからotherを成分ごとに引いた差。
   Vector4 operator-(const Vector4& other) const { return { x - other.x, y - other.y, z - other.z, w - other.w }; }

   /// @brief 全成分へ同じスカラーを乗算する。
   /// @param scalar 乗算する係数。
   /// @return スケーリング後のベクトル。
   Vector4 operator*(const float& scalar) const { return { x * scalar, y * scalar, z * scalar, w * scalar }; }

   /// @brief 行列の各行との内積によって4次元ベクトルを変換する。
   /// @param mat 適用する4x4行列。
   /// @return result[i] = mat.m[i][0] * x + ... + mat.m[i][3] * wとして求めたベクトル。
   /// @note 演算子の記法はvector * matrixだが、要素計算は行列の各行を左側に置く規約で行う。
   Vector4 operator*(const Matrix4x4& mat) const {
	  Vector4 result;
	  // 同次座標の情報を落とさないため、各行との内積にはwを含む4成分すべてを使う。
	  result.x = mat.m[0][0] * x + mat.m[0][1] * y + mat.m[0][2] * z + mat.m[0][3] * w;
	  result.y = mat.m[1][0] * x + mat.m[1][1] * y + mat.m[1][2] * z + mat.m[1][3] * w;
	  result.z = mat.m[2][0] * x + mat.m[2][1] * y + mat.m[2][2] * z + mat.m[2][3] * w;
	  result.w = mat.m[3][0] * x + mat.m[3][1] * y + mat.m[3][2] * z + mat.m[3][3] * w;
	  return result;
   }

   /// @brief 全成分を同じスカラーで除算し、このベクトルを更新する。
   /// @param scalar 除数。
   /// @return 更新後のこのベクトルへの参照。
   /// @pre scalarは0でないこと。
   Vector4& operator/=(float scalar) {
	  x /= scalar;
	  y /= scalar;
	  z /= scalar;
	  w /= scalar;
	  return *this;
   }
};

} // namespace GameEngine