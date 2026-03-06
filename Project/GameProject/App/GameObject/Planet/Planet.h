#pragma once
#include "../GameObject.h"

/// @brief 惑星オブジェクト
class Planet : public GameObject {
public:
   /// @brief 惑星の動きのタイプ
   enum class MovementType {
	  Static = 0,        // 静止
	  Orbit = 1,         // 円軌道で移動
	  Pendulum = 2,      // 振り子運動
	  Wave = 3           // 波のような動き
   };

   /// @brief 移動パラメータ
   struct MovementParams {
	  MovementType type = MovementType::Static;
	  GameEngine::Vector3 orbitCenter;       // 軌道の中心
	  float orbitRadius = 10.0f; // 軌道半径
	  float orbitSpeed = 1.0f;   // 速度
	  GameEngine::Vector3 orbitAxis;         // 回転軸（Orbit用）
	  GameEngine::Vector3 waveDirection;     // 波の方向（Wave用）
	  float waveAmplitude = 5.0f;// 波の振幅（Wave用）
	  float pendulumAngle = 45.0f;// 振り子の角度（Pendulum用）
	  float initialPhase = 0.0f; // 初期位相
   };

   void Initialize(GameEngine::Model* model, float gravitationalRadius, float planetRadius);
   void Initialize(GameEngine::Model* model, float gravitationalRadius, float planetRadius, const MovementParams& params);

   // オブジェクト固有の更新
   void Update(float dt) override;

   // 重力影響範囲の半径を取得
   float GetGravitationalRadius() const { return gravitationalRadius_; }
   // 重力影響範囲の半径を設定
   void SetGravitationalRadius(float r) { gravitationalRadius_ = r; }
   // 惑星の半径を取得
   float GetPlanetRadius() const { return planetRadius_; }
   // 惑星の半径を設定
   void SetPlanetRadius(float r) { planetRadius_ = r; }

   // 移動タイプを取得
   MovementType GetMovementType() const { return movementParams_.type; }
   // 移動パラメータを取得
   const MovementParams& GetMovementParams() const { return movementParams_; }
   
   // このフレームの移動量を取得（前フレームからの差分）
   GameEngine::Vector3 GetFrameMovement() const { return frameMovement_; }

private:
   // 各移動タイプの更新処理
   void UpdateOrbit(float dt);
   void UpdatePendulum(float dt);
   void UpdateWave(float dt);

   float gravitationalRadius_ = 1.0f;
   float planetRadius_ = 1.0f;
   MovementParams movementParams_;
   GameEngine::Vector3 initialPosition_;  // 初期位置（Static以外で使用）
   float elapsedTime_ = 0.0f; // 経過時間
   GameEngine::Vector3 previousPosition_; // 前フレームの位置
   GameEngine::Vector3 frameMovement_;    // このフレームの移動量
};