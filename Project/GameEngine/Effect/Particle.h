#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"

namespace GameEngine {
/// @brief 個別粒子データ
struct Particle {
   Transform transform;        // 位置・回転・スケール
   Transform simulationSpaceTransform; // 放出時のShape Transform
   Vector3 velocity;          // 速度
   Vector3 acceleration;      // 加速度
   Vector3 angularVelocity;   // 角速度（ランダム対応）
   Vector3 velocityOverLifetimeLinearVelocity;
   float velocityOverLifetimeSpeedModifier;
   Vector3 forceOverLifetimeForce;
   float limitVelocitySpeedLimit;
   float limitVelocityDampen;
   float noiseStrength;
   float noiseFrequency;
   float noiseScrollSpeed;
   float noiseTime;
   Vector4 color;            // 色（RGBA）
   float lifeTime;           // 寿命
   float currentTime;        // 現在経過時間
   Vector3 initialSize;       // 初期サイズ
   Vector3 currentSize;       // 現在のサイズ
   Vector2 uvOffset;          // UVオフセット
   Vector2 uvScale;           // UVスケール
   float uvRotation;          // UV回転
   Vector2 uvScroll;          // UVスクロール速度
   int sheetFrame;            // テクスチャシートの現在フレーム
   int sheetRow;              // テクスチャシートの行
   bool isActive;            // 有効フラグ

   Particle()
	  : simulationSpaceTransform()
	  , velocity(0.0f, 0.0f, 0.0f)
	  , acceleration(0.0f, 0.0f, 0.0f)
	  , angularVelocity(0.0f, 0.0f, 0.0f)
	  , velocityOverLifetimeLinearVelocity(0.0f, 0.0f, 0.0f)
	  , velocityOverLifetimeSpeedModifier(1.0f)
	  , forceOverLifetimeForce(0.0f, 0.0f, 0.0f)
	  , limitVelocitySpeedLimit(10.0f)
	  , limitVelocityDampen(0.5f)
	  , noiseStrength(1.0f)
	  , noiseFrequency(0.5f)
	  , noiseScrollSpeed(1.0f)
	  , noiseTime(0.0f)
	  , color(1.0f, 1.0f, 1.0f, 1.0f)
	  , lifeTime(1.0f)
	  , currentTime(0.0f)
	  , initialSize(1.0f, 1.0f, 1.0f)
	  , currentSize(1.0f, 1.0f, 1.0f)
	  , uvOffset(0.0f, 0.0f)
	  , uvScale(1.0f, 1.0f)
	  , uvRotation(0.0f)
	  , uvScroll(0.0f, 0.0f)
	  , sheetFrame(0)
	  , sheetRow(0)
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
