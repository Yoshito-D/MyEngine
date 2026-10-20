#pragma once

#include <cstdint>
#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include <vector>

namespace GameEngine {

/// @brief 個別粒子データ
struct Particle {
   Transform transform;                // 位置・回転・スケール
   Transform simulationSpaceTransform; // 放出時のShape Transform
   Vector3 velocity;                   // 速度
   Vector3 acceleration;               // 加速度
   Vector3 angularVelocity;            // 角速度（ランダム対応）
   Vector3 velocityOverLifetimeLinearVelocity;
   float velocityOverLifetimeSpeedModifier;
   Vector3 forceOverLifetimeForce;
   float limitVelocitySpeedLimit;
   float limitVelocityDampen;
   float noiseStrength;
   float noiseFrequency;
   float noiseScrollSpeed;
   float noiseTime;
   Vector4 color;                     // 色（RGBA）
   Vector4 customData;                // シェーダー連携用（年齢、乱数、カメラフェード、速度）
   std::vector<Vector3> ribbonPoints; // リボン生成用のワールド位置履歴
   float ribbonWidth;
   float subEmitterTimer;
   float gravityModifier;
   float drag;
   float sizeOverLifetimeMultiplier;
   float lifeTime;          // 寿命
   float currentTime;       // 現在経過時間
   Vector3 initialSize;     // 初期サイズ
   Vector3 currentSize;     // 現在のサイズ
   Vector2 uvOffset;        // UVオフセット
   Vector2 uvScale;         // UVスケール
   float uvRotation;        // UV回転
   Vector2 uvScroll;        // UVスクロール速度
   std::int32_t sheetFrame; // テクスチャシートの現在フレーム
   std::int32_t sheetRow;   // テクスチャシートの行
   bool isActive;           // 有効フラグ

   Particle();

   /// @brief 粒子の寿命進行度を取得（0.0 ～ 1.0）
   /// @return 寿命進行度
   float GetLifeProgress() const;

   /// @brief 粒子が生きているか判定
   /// @return 生存しているかどうか
   bool IsAlive() const { return isActive && currentTime < lifeTime; }
};
}
