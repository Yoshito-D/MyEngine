#pragma once
#include <algorithm>
#include <cmath>
#include "Vector2.h"
#include "Quaternion.h"

namespace GameEngine {

/// @brief 3次元空間の座標、方向、Euler角などを表す浮動小数点ベクトル。
struct Vector3 {
   float x, y, z;

   /// @brief 2点間を線形補間する。
   /// @param start 補間区間の始点。
   /// @param end 補間区間の終点。
   /// @param t 補間係数。0以下はstart、1以上はendへクランプされる。
   /// @return start + (end - start) * tで求めたベクトル。
   static Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
	  t = std::clamp(t, 0.0f, 1.0f);
	  return start + (end - start) * t;
   }

   /// @brief 2つの方向を単位球面上で補間する。
   /// @param start 補間開始方向。
   /// @param end 補間終了方向。
   /// @param t 補間係数。0から1の範囲へクランプされる。
   /// @return 補間された方向。通常は単位ベクトルだが、退化入力のフォールバックではその限りではない。
   /// @details 非零入力は内部で正規化するため、startとendの長さは結果へ引き継がれない。
   /// @note 反対向きの方向間では球面上の経路が一意でなく、正規化線形補間へのフォールバックが中点で0ベクトルを返す場合がある。
   static Vector3 Slerp(const Vector3& start, const Vector3& end, float t) {
	  t = std::clamp(t, 0.0f, 1.0f);

	  // 球面補間には方向が必要なため、極小ベクトルを含む場合は元の長さを保つ線形補間へ退避する。
	  if (start.LengthSquared() < 1e-8f || end.LengthSquared() < 1e-8f) {
		 return Lerp(start, end, t);
	  }

	  Vector3 normalizedStart = start.Normalize();
	  Vector3 normalizedEnd = end.Normalize();
	  float dot = std::clamp(normalizedStart.Dot(normalizedEnd), -1.0f, 1.0f);

	  // 方向が近いとsin(theta)による除算が不安定になるため、正規化線形補間で連続性を保つ。
	  if (dot > 0.9995f) {
		 return Lerp(normalizedStart, normalizedEnd, t).Normalize();
	  }

	  float theta = std::acos(dot);
	  float sinTheta = std::sin(theta);
	  // 主に反対向きで回転面を決められない場合を処理し、極小値による除算を避ける。
	  if (std::abs(sinTheta) < 1e-5f) {
		 return Lerp(normalizedStart, normalizedEnd, t).Normalize();
	  }

	  return (normalizedStart * std::sin((1.0f - t) * theta)
		 + normalizedEnd * std::sin(t * theta))
		 / sinTheta;
   }

   /// @brief ベクトルを成分ごとに加算する。
   /// @param vector 加算するベクトル。
   /// @return 成分ごとの和。
   Vector3 operator+(const Vector3& vector) const { return { x + vector.x, y + vector.y, z + vector.z }; }

   /// @brief ベクトルを成分ごとに減算する。
   /// @param vector 減算するベクトル。
   /// @return 成分ごとの差。
   Vector3 operator-(const Vector3& vector) const { return { x - vector.x, y - vector.y, z - vector.z }; }

   /// @brief 全成分へスカラーを乗算する。
   /// @param scalar 乗算する係数。
   /// @return スケーリング後のベクトル。
   Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }

   /// @brief 全成分をスカラーで除算する。
   /// @param scalar 除数。
   /// @return 除算後のベクトル。
   /// @pre scalarは0でないこと。関数内ではゼロ除算を検査しない。
   Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }

   /// @brief ベクトルを成分ごとに加算し、このベクトルを更新する。
   /// @param vector 加算するベクトル。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator+=(const Vector3& vector) { x += vector.x; y += vector.y; z += vector.z; return *this; }

   /// @brief 各成分へ同じスカラーを加算し、このベクトルを更新する。
   /// @param scalar 加算する値。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator+=(const float scalar) { x += scalar;	y += scalar; z += scalar; return *this; }

   /// @brief ベクトルを成分ごとに減算し、このベクトルを更新する。
   /// @param vector 減算するベクトル。
   /// @return 更新後のベクトルのコピー。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator-=(const Vector3& vector) { x -= vector.x; y -= vector.y; z -= vector.z; return *this; }

   /// @brief 全成分の符号を反転する。
   /// @return このベクトルと逆向きのベクトル。
   Vector3 operator-() const { return { -x, -y, -z }; }

   /// @brief 全成分をスカラーで除算し、このベクトルを更新する。
   /// @param scalar 除数。
   /// @return 更新後のベクトルのコピー。
   /// @pre scalarは0でないこと。関数内ではゼロ除算を検査しない。
   /// @note 戻り値は参照ではないため、複合代入を連鎖する用途には適さない。
   Vector3 operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

   /// @brief このベクトルとvectorの外積を求める。
   /// @param vector 右オペランドのベクトル。
   /// @return 両ベクトルに直交するthis x vector。オペランドの順序を逆にすると符号も反転する。
   Vector3 Cross(const Vector3& vector) const { return { y * vector.z - z * vector.y, z * vector.x - x * vector.z, x * vector.y - y * vector.x }; }

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
   Vector3 Normalize() const {
	  float length = Length();
	  // 0除算チェック：長さが極小の場合は0ベクトルを返す
	  if (length < 1e-8f) {
		 return { 0.0f, 0.0f, 0.0f };
	  }
	  return { x / length, y / length, z / length };
   }

   /// @brief このベクトルを指定ベクトルの方向へ正射影する。
   /// @param vector 射影先の方向。長さは結果に影響しない。
   /// @return vectorを正規化した方向へ投影したベクトル。vectorが極小なら0ベクトル。
   Vector3 Project(const Vector3& vector) const { Vector3 normalized = vector.Normalize(); return normalized * Dot(normalized); }

   /// @brief このベクトルに直交するベクトルを1つ求める。
   /// @return 内積が0になるベクトル。長さは正規化されず、このベクトルが0なら0ベクトル。
   /// @details XY成分のどちらかが非零ならXY平面内の直交方向を使い、両方が0ならYZ平面内で構成する。
   Vector3 Perpendicular() const { if (x != 0.0f || y != 0.0f) { return { -y,x,0.0f }; } return { 0.0f,-z,y }; }

   /// @brief このベクトルをラジアン単位のXYZ Euler角としてQuaternionへ変換する。
   /// @return 各軸角の半角三角関数から構成した回転Quaternion。
   /// @note 浮動小数点誤差まで含めた単位長を保証する必要がある場合は、戻り値をNormalizeする。
   Quaternion ToQuaternion() const {
	  float halfX = x * 0.5f;
	  float halfY = y * 0.5f;
	  float halfZ = z * 0.5f;
	  
	  float cosX = std::cos(halfX);
	  float cosY = std::cos(halfY);
	  float cosZ = std::cos(halfZ);
	  float sinX = std::sin(halfX);
	  float sinY = std::sin(halfY);
	  float sinZ = std::sin(halfZ);

	  return Quaternion{
		  sinX * cosY * cosZ - cosX * sinY * sinZ,
		  cosX * sinY * cosZ + sinX * cosY * sinZ,
		  cosX * cosY * sinZ - sinX * sinY * cosZ,
		  cosX * cosY * cosZ + sinX * sinY * sinZ
	  };
   }

};

} // namespace GameEngine