#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"

namespace GameEngine {
/// @brief 個別粒子データ
struct Particle {
   Transform transform;        // 位置・回転・スケール
   Vector3 velocity;          // 速度
   Vector3 acceleration;      // 加速度
   Vector3 angularVelocity;   // 角速度（ランダム対応）
   Vector4 color;            // 色（RGBA）
   float lifeTime;           // 寿命
   float currentTime;        // 現在経過時間
   float initialSize;        // 初期サイズ
   float currentSize;        // 現在のサイズ
   bool isActive;            // 有効フラグ

   Particle()
	  : velocity(0.0f, 0.0f, 0.0f)
	  , acceleration(0.0f, 0.0f, 0.0f)
	  , angularVelocity(0.0f, 0.0f, 0.0f)
	  , color(1.0f, 1.0f, 1.0f, 1.0f)
	  , lifeTime(1.0f)
	  , currentTime(0.0f)
	  , initialSize(1.0f)
	  , currentSize(1.0f)
	  , isActive(false) {}

   /// @brief 粒子の寿命進行度を取得（0.0 ～ 1.0）
   /// @return 寿命進行度
   float GetLifeProgress() const {
	  if (lifeTime <= 0.0f) return 1.0f;
	  return currentTime / lifeTime;
   }

   /// @brief 粒子が生きているか判定
   /// @return 生存しているかどうか
   bool IsAlive() const {
	  return isActive && currentTime < lifeTime;
   }
};
}
