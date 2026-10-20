#pragma once

#include <cmath>

namespace GameEngine {

/// @brief 2次元空間の座標や方向を表す浮動小数点ベクトル。
struct Vector2 {
   float x; //!< X成分
   float y; //!< Y成分

   /// @brief 各成分を加算する。
   /// @param other 加算するベクトル。
   /// @return このベクトルとotherの成分ごとの和。
   Vector2 operator+(const Vector2& other) const { return {x + other.x, y + other.y}; }

   /// @brief 各成分を減算する。
   /// @param other 減算するベクトル。
   /// @return このベクトルからotherを成分ごとに引いた差。
   Vector2 operator-(const Vector2& other) const { return {x - other.x, y - other.y}; }

   /// @brief 全成分へ同じスカラーを乗算する。
   /// @param scalar 乗算する係数。
   /// @return スケーリング後のベクトル。
   Vector2 operator*(float scalar) const { return {x * scalar, y * scalar}; }

   /// @brief 全成分を同じスカラーで除算する。
   /// @param scalar 除数。
   /// @return 除算後のベクトル。
   /// @note scalarが0の場合はゼロベクトルを返す。
   Vector2 operator/(float scalar) const;

   /// @brief otherを成分ごとに加算し、このベクトルを更新する。
   /// @param other 加算するベクトル。
   /// @return 更新後のベクトルのコピー。
   /// @note 一般的な複合代入演算子と異なり、戻り値は参照ではない。
   Vector2 operator+=(const Vector2& other);

   /// @brief 全成分へスカラーを乗算し、このベクトルを更新する。
   /// @param scalar 乗算する係数。
   /// @return 更新後のベクトルのコピー。
   /// @note 一般的な複合代入演算子と異なり、戻り値は参照ではない。
   Vector2 operator*=(float scalar);

   /// @brief 向きを保った単位ベクトルを求める。
   /// @return 長さ1のベクトル。入力が0ベクトルの場合は(0, 0)を返す。
   Vector2 Normalize() const;

   /// @brief ユークリッド長を求める。
   /// @return sqrt(x * x + y * y)で計算した非負の長さ。
   float Length() const { return std::sqrt(x * x + y * y); }
};

} // namespace GameEngine
