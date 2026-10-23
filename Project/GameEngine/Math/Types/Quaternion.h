#pragma once

#include <algorithm>
#include <cmath>

namespace GameEngine {
struct Vector3;

/// @brief 3次元回転を表すQuaternion。
/// @details x、y、zをベクトル部、wをスカラー部として保持する。回転として使用するAPIは原則として単位Quaternionを前提とする。
struct Quaternion {
   float x; ///< 回転クォータニオンの虚部X成分。
   float y; ///< 回転クォータニオンの虚部Y成分。
   float z; ///< 回転クォータニオンの虚部Z成分。
   float w; ///< 回転クォータニオンの実部。

   /// @brief 2つの回転を正規化線形補間する。
   /// @param start 補間開始のQuaternion。
   /// @param end 補間終了のQuaternion。
   /// @param t 補間係数。0から1の範囲へクランプされる。
   /// @return 最短側の符号を選んで線形補間し、正規化したQuaternion。
   /// @pre startとendは回転を表す単位Quaternionであること。
   static Quaternion Lerp(const Quaternion& start, const Quaternion& end, float t);

   /// @brief 2つの回転を球面線形補間する。
   /// @param start 補間開始のQuaternion。
   /// @param end 補間終了のQuaternion。
   /// @param t 補間係数。0から1の範囲へクランプされる。
   /// @return 最短経路を一定角速度で補間し、正規化したQuaternion。
   /// @pre startとendは回転を表す単位Quaternionであること。入力自体は関数内で正規化されない。
   static Quaternion Slerp(const Quaternion& start, const Quaternion& end, float t);

   /// @brief 無回転を表す単位Quaternionを生成する。
   /// @return (0, 0, 0, 1)。
   static Quaternion Identity() { return Quaternion{0.0f, 0.0f, 0.0f, 1.0f}; }

   /// @brief Quaternionを成分ごとに加算する。
   /// @param q 加算するQuaternion。
   /// @return 各成分の和。結果は自動では正規化されない。
   Quaternion operator+(const Quaternion& q) const { return Quaternion{x + q.x, y + q.y, z + q.z, w + q.w}; }

   /// @brief Quaternionを成分ごとに減算する。
   /// @param q 減算するQuaternion。
   /// @return 各成分の差。結果は自動では正規化されない。
   Quaternion operator-(const Quaternion& q) const { return Quaternion{x - q.x, y - q.y, z - q.z, w - q.w}; }

   /// @brief 全成分の符号を反転する。
   /// @return (-x, -y, -z, -w)。単位Quaternionでは元と同じ回転を表す。
   Quaternion operator-() const { return Quaternion{-x, -y, -z, -w}; }

   /// @brief このQuaternionとqのHamilton積を求める。
   /// @param q 右オペランドのQuaternion。
   /// @return this * qで合成したQuaternion。
   /// @details QuaternionOperations::RotateVectorは、ベクトルを純Quaternion pへ変換し、rotation * p * rotation.Conjugate()の順で回転する。
   /// @note Quaternion積は交換可能ではなく、結果は自動では正規化されない。
   Quaternion operator*(const Quaternion& q) const;

   /// @brief 全成分へスカラーを乗算する。
   /// @param t 乗算する係数。
   /// @return スケーリング後のQuaternion。回転として使う場合は必要に応じて正規化する。
   Quaternion operator*(float t) const { return Quaternion{x * t, y * t, z * t, w * t}; }

   /// @brief ベクトル部の符号を反転した共役Quaternionを求める。
   /// @return (-x, -y, -z, w)。
   /// @note 単位Quaternionでは共役が逆回転と一致する。非単位入力の逆元にはInverseを使用する。
   Quaternion Conjugate() const { return Quaternion(-x, -y, -z, w); }

   /// @brief 4成分ベクトルとしてのノルムを求める。
   /// @return sqrt(x * x + y * y + z * z + w * w)で計算した非負のノルム。
   float Norm() const { return std::sqrt(x * x + y * y + z * z + w * w); }

   /// @brief 同じ回転方向を保った単位Quaternionを求める。
   /// @return ノルムで各成分を除算したQuaternion。ノルムが1e-8未満ならIdentity。
   Quaternion Normalize() const;

   /// @brief 乗算するとIdentityになる逆Quaternionを求める。
   /// @return 共役をノルムの2乗で除算したQuaternion。ノルムの2乗が1e-8未満ならIdentity。
   /// @note 単位Quaternionに限らず逆元を計算するが、極小入力はゼロ除算を避けるためIdentityへフォールバックする。
   Quaternion Inverse() const;

   /// @brief 4成分ベクトルとしての内積を求める。
   /// @param q 内積を取るQuaternion。
   /// @return x * q.x + y * q.y + z * q.z + w * q.w。
   /// @details 単位Quaternion同士では回転間の角度判定や最短補間方向の選択に使用できる。
   float Dot(const Quaternion& q) const { return x * q.x + y * q.y + z * q.z + w * q.w; }

   /// @brief 回転QuaternionをXYZ Euler角へ変換する。
   /// @return ラジアン単位の(X軸角, Y軸角, Z軸角)。
   /// @pre このQuaternionは正規化済みであること。関数内では正規化しない。
   /// @note Euler角表現は一意ではなく、Y軸角が特異姿勢に近い場合はジンバルロックの影響を受ける。
   Vector3 ToEuler() const;
};

} // namespace GameEngine
