#pragma once

#include "Vector3.h"
#include "Quaternion.h"

#include <cmath>

namespace GameEngine {

/// @brief 3次元オブジェクトの拡大縮小・回転・平行移動をまとめた変換情報。
/// @details 回転はEuler角とQuaternionの両方を保持し、rotationSourceで選択した表現を正として利用する。
///          公開メンバーを直接変更しても他方の回転表現は同期されないため、回転の切り替えにはSetRotation系を使用する。
struct Transform {
   /// @brief 現在の姿勢を決定する回転表現。
   enum class RotationSource {
      Euler, //!< rotationのEuler角を使用する
      Quaternion //!< rotationQuaternionを使用する
   };

   Vector3 scale; //!< 各ローカル軸に適用する拡大率
   Vector3 rotation; //!< ラジアン単位のXYZ Euler角
   Vector3 translation; //!< 変換へ適用する平行移動量
   Quaternion rotationQuaternion; //!< Quaternion形式で保持する姿勢
   RotationSource rotationSource; //!< 変換行列の構築時に優先する回転表現

   /// @brief 単位スケール・無回転・原点位置の恒等変換を生成する。
   /// @details 初期状態ではEuler角を有効な回転表現とする。
   Transform()
      : scale(1.0f, 1.0f, 1.0f)
      , rotation(0.0f, 0.0f, 0.0f)
      , translation(0.0f, 0.0f, 0.0f)
      , rotationQuaternion(Quaternion::Identity())
      , rotationSource(RotationSource::Euler) {
   }

   /// @brief Euler角を有効な回転として設定する。
   /// @param euler ラジアン単位のXYZ Euler角。
   /// @details Quaternion側も同じ姿勢へ同期し、後から表現を参照・切り替えた際の不整合を防ぐ。
   void SetRotationEuler(const Vector3& euler) {
      // シリアライズや編集UIがどちらの表現を読んでも同じ姿勢になるよう、非選択側も更新する。
      rotation = euler;
      rotationQuaternion = EulerToQuaternion(euler);
      rotationSource = RotationSource::Euler;
   }

   /// @brief Quaternionを有効な回転として設定する。
   /// @param quaternion 設定する姿勢。非単位Quaternionは内部で正規化される。
   /// @details Euler角側も同じ姿勢へ同期し、保存や編集UIが参照する値を最新に保つ。
   void SetRotationQuaternion(const Quaternion& quaternion) {
      // 回転としての大きさを1に固定してから、表示・保存用のEuler角へ変換する。
      rotationQuaternion = quaternion.Normalize();
      rotation = QuaternionToEuler(rotationQuaternion);
      rotationSource = RotationSource::Quaternion;
   }

   /// @brief Quaternionが現在の有効な回転表現かを調べる。
   /// @return rotationQuaternionを使用する場合はtrue、rotationを使用する場合はfalse。
   bool IsUsingQuaternion() const {
      return rotationSource == RotationSource::Quaternion;
   }

   /// @brief 現在有効な姿勢を単位Quaternionとして取得する。
   /// @return 選択中の回転表現から得た正規化済みQuaternion。
   Quaternion GetActiveQuaternion() const {
      if (rotationSource == RotationSource::Quaternion) {
         return rotationQuaternion.Normalize();
      }
      // 公開のrotationが直接編集される場合があるため、保存済みQuaternionではなく現在値から再構築する。
      return EulerToQuaternion(rotation);
   }

   /// @brief 現在有効な姿勢をEuler角として取得する。
   /// @return ラジアン単位のXYZ Euler角。
   Vector3 GetActiveEuler() const {
      if (rotationSource == RotationSource::Quaternion) {
         // Quaternion側を正として変換し、同期後にrotationだけが古くなっていても姿勢へ影響させない。
         return QuaternionToEuler(rotationQuaternion);
      }
      return rotation;
   }

private:
   static Quaternion EulerToQuaternion(const Vector3& eulerAngles) {
      return eulerAngles.ToQuaternion().Normalize();
   }

   static Vector3 QuaternionToEuler(const Quaternion& quaternion) {
      return quaternion.Normalize().ToEuler();
   }
};

} // namespace GameEngine
