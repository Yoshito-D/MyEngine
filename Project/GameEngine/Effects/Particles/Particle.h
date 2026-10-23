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
   Vector3 velocityOverLifetimeLinearVelocity; ///< 生成時に確定した粒子固有の速度加算値。
   float velocityOverLifetimeSpeedModifier;    ///< 生成時に確定した粒子固有の速度倍率。
   Vector3 forceOverLifetimeForce;             ///< 生成時に確定した粒子固有の外力ベクトル。
   float limitVelocitySpeedLimit;              ///< 粒子速度を制限する上限値。
   float limitVelocityDampen;                  ///< 上限を超えた速度を抑える減衰割合。
   float noiseStrength;                        ///< 速度へ加えるノイズの強さ。
   float noiseFrequency;                       ///< 位置に対するノイズの空間周波数。
   float noiseScrollSpeed;                     ///< 時間に対するノイズ位相の進行速度。
   float noiseTime;                            ///< ノイズ位相を評価する経過時間（秒）。
   Vector4 color;                     // 色（RGBA）
   Vector4 customData;                // シェーダー連携用（年齢、乱数、カメラフェード、速度）
   std::vector<Vector3> ribbonPoints; // リボン生成用のワールド位置履歴
   float ribbonWidth;                 ///< この粒子の履歴から生成するリボンの幅。
   float subEmitterTimer;             ///< 次の更新時サブエミッター発火まで繰り越す経過秒数。
   float gravityModifier;             ///< 重力加速度へ掛ける倍率。
   float drag;                        ///< 速度を時間に応じて減衰させる抵抗係数。
   float sizeOverLifetimeMultiplier;  ///< 寿命に応じたサイズへ掛ける粒子固有の倍率。
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
