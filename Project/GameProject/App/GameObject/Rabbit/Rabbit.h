#pragma once
#include "../GameObject.h"
#include "../Planet/Planet.h"
#include "Utility/Math/Quaternion.h"
#include "../../Component/SphericalMovementComponent.h"
#include "../../Component/GravityComponent.h"
#include <memory>
#include <functional>

// 前方宣言
class Player;

namespace GameEngine {
   class ParticleSystem;
   class PointLight;
}

class Rabbit : public GameObject {
public:
   void Initialize(GameEngine::Model* model, Planet* planet, float radius);
   void Update(float dt) override;

   void OnCollisionEnter(GameObject* other) override;
   void OnCollisionStay(GameObject* other) override;
   void OnCollisionExit(GameObject* other) override;

   void SetPlayer(Player* player) { player_ = player; }
   
   // 所属する惑星を取得
   Planet* GetPlanet() const { return sphericalMovement_.GetCurrentPlanet(); }
   
   // ラビットの半径を取得
   float GetRabbitRadius() const { return rabbitRadius_; }
   
   // パーティクルシステムを設定
   void SetParticleSystem(GameEngine::ParticleSystem* particles) { captureParticles_ = particles; }
   
   // 捕獲時のコールバックを設定
   void SetCaptureCallback(std::function<void(const GameEngine::Vector3&)> callback) { captureCallback_ = callback; }
   
   // 捕獲演出を開始
   void StartCaptureEffect();
   
   // 捕獲演出中かどうか
   bool IsPlayingCaptureEffect() const { return isPlayingCaptureEffect_; }

   // デバッグ情報表示
   void ShowDebugInfo();

private:
   // ステータマシン初期化
   void InitializeStateMachine();
   
   // 各ステートの処理
   void OnEnterIdle();
   void OnUpdateIdle();
   
   void OnEnterWandering();
   void OnUpdateWandering();
   
   void OnEnterFleeing();
   void OnUpdateFleeing();

   // 共通処理
   void UpdateMovement(float dt);
   
   // AI判断
   void CheckPlayerDistance();
   bool IsPlayerInFront() const;  // プレイヤーが前方にいるかチェック
   void ChooseRandomDirection();

private:
   Player* player_ = nullptr;
   
   // コンポーネント
   SphericalMovementComponent sphericalMovement_;
   GravityComponent gravity_;
   
   // 移動パラメータ
   GameEngine::Vector3 moveDirection_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);
   GameEngine::Vector3 targetDirection_ = GameEngine::Vector3(0.0f, 0.0f, 1.0f); // 目標移動方向
   float moveSpeed_ = 9.5f;
   float fleeSpeed_ = 18.0f;
   float jumpPower_ = 6.5f;
   float turnSpeed_ = 8.0f;  // 向き変更速度（ラジアン/秒）
   
   float rabbitRadius_ = 1.0f;
   
   // AI状態管理
   float detectionRadius_ = 4.0f;  // プレイヤー検知範囲（逃走開始判定のみ使用）
   float fleeRadius_ = 4.0f;       // 逃走開始距離
   bool isPlayerNear_ = false;
   bool isCurrentlyFleeing_ = false;  // 現在逃走中かどうかのフラグ
   
   // 逃走状態管理
   float fleeTimer_ = 0.0f;  // 逃走開始からの経過時間
   float minFleeDuration_ = 4.0f;  // 逃走継続時間（10秒固定）
   GameEngine::Vector3 initialFleeDirection_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);  // 最初に逃げた方向を保持
   
   // 徘徊用タイマー
   float wanderTimer_ = 0.0f;
   float wanderDuration_ = 2.0f;   // 徘徊継続時間
   float idleTimer_ = 0.0f;
   float idleDuration_ = 6.0f;     // 待機時間（長めに）
   float directionChangeTimer_ = 0.0f;  // 方向転換タイマー
   float idleTurnTimer_ = 0.0f;  // 待機状態での振り向きタイマー
   float idleTurnInterval_ = 3.0f;  // 待機状態での振り向き間隔
   
   // 徘徊確率
   float wanderChance_ = 0.0f; 
   
   // ジャンプ用タイマー
   float jumpTimer_ = 0.0f;
   float jumpInterval_ = 0.1f;     // ジャンプ間隔
   
   // 捕獲演出関連
   GameEngine::ParticleSystem* captureParticles_ = nullptr;
   std::function<void(const GameEngine::Vector3&)> captureCallback_ = nullptr;  // 捕獲時のコールバック（位置を渡す）
   bool isPlayingCaptureEffect_ = false;
   
   // ラビット専用ポイントライト
   GameEngine::PointLight* rabbitPointLight_ = nullptr;
};