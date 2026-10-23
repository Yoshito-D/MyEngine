#pragma once

#include <algorithm>
#include <cmath>
#include "GameEngine/Math/Types/Vector2.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace GameEngine {

/// @brief 3次元空間の座標、方向、Euler角などを表す浮動小数点ベクトル。
struct Vector3 {
   float x, y, z; ///< 3次元ベクトルのX・Y・Z成分。

   /// @brief 2点間を線形補間する。
   /// @param start 補間区間の始点。
   /// @param end 補間区間の終点。
   /// @param t 補間係数。0以下はstart、1以上はendへクランプされる。
   /// @return start + (end - start) * tで求めたベクトル。
   static Vector3 Lerp(const Vector3& start, const Vector3& end, float t);

   /// @brief 2つの方向を単位球面上で補間する。
   /// @param start 補間開始方向。
   /// @param end 補間終了方向。
   /// @param t 補間係数。0から1の範囲へクランプされる。
   /// @return 補間された方向。通常は単位ベクトルだが、退化入力のフォールバックではその限りではない。
   /// @details 非零入力は内部で正規化するため、startとendの長さは結果へ引き継がれない。
   /// @note 反対向きの方向間では球面上の経路が一意でなく、正規化線形補間へのフォールバックが中点で0ベクトルを返す場合がある。
   static Vector3 Slerp(const Vector3& start, const Vector3& end, float t);

   /// @brief ベクトルを成分ごとに加算する。
   /// @param vector 加算するベクトル。
   /// @return 成分ごとの和。
   Vector3 operator+(const Vector3& vector) const { return {x + vector.x, y + vector.y, z + vector.z}; }

   /// @brief ベクトルを成分ごとに減算する。
   /// @param vector 減算するベクトル。
   /// @return 成分ごとの差。
   Vector3 operator-(const Vector3& vector) const { return {x - vector.x, y - vector.y, z - vector.z}; }

   /// @brief 全成分へスカラーを乗算する。
   /// @param scalar 乗算する係数。
   /// @return スケーリング後のベクトル。
   Vector3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }

   /// @brief 全成分をスカラーで除算する。
   /// @param scalar 除数。
   /// @return 除算後のベクトル。
   /// @pre scalarは0でないこと。関数内ではゼロ除算を検査しない。
   Vector3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }

   /// @brief ベクトルを成分ごとに加算し、このベクトルを更新する。
   /// @param vector 加算するベクトル。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator+=(const Vector3& vector);

   /// @brief 各成分へ同じスカラーを加算し、このベクトルを更新する。
   /// @param scalar 加算する値。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator+=(const float scalar);

   /// @brief ベクトルを成分ごとに減算し、このベクトルを更新する。
   /// @param vector 減算するベクトル。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator-=(const Vector3& vector);

   /// @brief 全成分の符号を反転する。
   /// @return このベクトルと逆向きのベクトル。
   Vector3 operator-() const { return {-x, -y, -z}; }

   /// @brief 全成分をスカラーで除算し、このベクトルを更新する。
   /// @param scalar 除数。
   /// @return 更新後のベクトルのコピー。
   /// @pre scalarは0でないこと。関数内ではゼロ除算を検査しない。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator/=(float scalar);

   /// @brief このベクトルとvectorの外積を求める。
   /// @param vector 右オペランドのベクトル。
   /// @return 両ベクトルに直交するthis x vector。オペランドの順序を逆にすると符号も反転する。
   Vector3 Cross(const Vector3& vector) const;

   /// @brief このベクトルとvectorの内積を求める。
   /// @param vector 内積を取るベクトル。
   /// @return x * vector.x + y * vector.y + z * vector.z。
   float Dot(const Vector3& vector) const { return x * vector.x + y * vector.y + z * vector.z; }

   /// @brief ユークリッド長を求める。
   /// @return sqrt(x * x + y * y + z * z)で計算した非負の長さ。
   float Length() const { return std::sqrt(x * x + y * y + z * z); }

   /// @brief 平方根を取らずに長さの2乗を求める。
   /// @return x * x + y * y + z * z。
   /// @details 距離比較など、実際の長さが不要な処理で平方根の計算を避けられる。
   float LengthSquared() const { return x * x + y * y + z * z; }

   /// @brief 向きを保った単位ベクトルを求める。
   /// @return 正規化したベクトル。長さが1e-8未満の場合は0ベクトル。
   Vector3 Normalize() const;

   /// @brief このベクトルを指定ベクトルの方向へ正射影する。
   /// @param vector 射影先の方向。長さは結果に影響しない。
   /// @return vectorを正規化した方向へ投影したベクトル。vectorが極小なら0ベクトル。
   Vector3 Project(const Vector3& vector) const;

   /// @brief このベクトルに直交するベクトルを1つ求める。
   /// @return 内積が0になるベクトル。長さは正規化されず、このベクトルが0なら0ベクトル。
   /// @details XY成分のどちらかが非零ならXY平面内の直交方向を使い、両方が0ならYZ平面内で構成する。
   Vector3 Perpendicular() const;

   /// @brief このベクトルをラジアン単位のXYZ Euler角としてQuaternionへ変換する。
   /// @return 各軸角の半角三角関数から構成した回転Quaternion。
   /// @note 浮動小数点誤差まで含めた単位長を保証する必要がある場合は、戻り値をNormalizeする。
   Quaternion ToQuaternion() const;
};

} // namespace GameEngine
