#pragma once
#include "../GameObject.h"
#include "Framework/EngineContext.h"
#include "../Planet/Planet.h"
#include "Core/Input/KeyConfig.h"
#include "Utility/Math/Quaternion.h"
#include "../../Component/SphericalMovementComponent.h"
#include "../../Component/GravityComponent.h"
#include "../../Collider/Collider.h"
#include "../../Collider/SphereCollider.h"
#include <memory>
#include <functional>
#include <vector>

// 前方宣言
class TPSCameraController;

/// @brief プレイヤーオブジェクト
class Player : public GameObject {
public:
   void Initialize(GameEngine::Model* model, float radius);
   // オブジェクト固有の更新
   void Update(float dt) override;

   void SetCurrentPlanet(Planet* planet);
   Planet* GetCurrentPlanet() const;

   void SetCameraController(TPSCameraController* controller) { cameraController_ = controller; }

   // 全惑星のリストを設定（最近接惑星検索用）
   void SetAllPlanets(const std::vector<Planet*>& planets) { allPlanets_ = planets; }

   void OnCollisionEnter(GameObject* other) override;
   void OnCollisionStay(GameObject* other) override;
   void OnCollisionExit(GameObject* other) override;

   // デバッグ情報表示
   void ShowDebugInfo();

   // デバッグ用：経度線と緯度線を描画
   void DrawGreatCircles();

   // Quaternionで回転を取得
   GameEngine::Quaternion GetRotationQuaternion() const;

   GameEngine::Vector3 GetForward() const;

   void SetMoveCallback(std::function<void(const GameEngine::Vector3&)> callback) { moveCallback_ = callback; }
   void SetStopCallback(std::function<void()> callback) { stopCallback_ = callback; }
   void SetJumpCallback(std::function<void(const GameEngine::Vector3&)> callback) { jumpCallback_ = callback; }
   void SetLandCallback(std::function<void(const GameEngine::Vector3&)> callback) { landCallback_ = callback; }
   void SetRabbitCaptureCallback(std::function<void(class Rabbit*)> callback) { rabbitCaptureCallback_ = callback; }
   
   // スピン中かどうかを取得
   bool IsSpinning() const { return isSpinning_; }
   
   // スピンコライダーを取得
   Collider* GetSpinCollider() const { return static_cast<Collider*>(spinCollider_.get()); }

private:
   // ステートマシン初期化
   void InitializeStateMachine();
   
   // 各ステートの処理
   void OnEnterIdle();
   void OnUpdateIdle();
   
   void OnEnterWalking();
   void OnUpdateWalking();
   
   void OnEnterJumping();
   void OnUpdateJumping();
   
   void OnEnterFalling();
   void OnUpdateFalling();

   // 惑星切り替え時の状態
   void OnEnterPlanetTransition();
   void OnUpdatePlanetTransition();
   
   // スピン状態
   void OnEnterSpinning();
   void OnUpdateSpinning();

   // 共通処理
   void UpdateMovement(float dt);
   void CheckJumpInput();
   void CheckMovementInput();
   void CheckSpinInput();

   // 惑星切り替え処理
   void InitiatePlanetSwitch(Planet* newPlanet);
   
   // 最近接惑星を探す
   Planet* FindNearestPlanetInRange(float maxDistance);
   
   // 惑星の重力範囲内にいるかチェック
   bool IsInsideAnyGravityField();

   // プレイヤーライトの更新
   void UpdatePlayerLight();

private:
   TPSCameraController* cameraController_ = nullptr;
   std::unique_ptr<GameEngine::KeyConfig> keyConfig_ = nullptr;

   // コンポーネント
   SphericalMovementComponent sphericalMovement_;
   GravityComponent gravity_;

   // 移動パラメータ
   GameEngine::Vector3 moveDirection_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);  // 入力方向
   GameEngine::Vector3 lastMoveDirection_ = GameEngine::Vector3(0.0f, 0.0f, 1.0f);  // 最後の移動方向（ワールド空間）
   float moveSpeed_ = 11.0f;
   float jumpPower_ = 20.0f;
   float turnSpeed_ = 10.0f;  // 向き変更速度（ラジアン/秒）

   float playerRadius_ = 0.5f;

   // デバッグ用
   float distanceToPlanet_ = 0.0f;

   // 入力フラグ（ステート遷移用）
   bool jumpRequested_ = false;
   bool hasMovementInput_ = false;
   bool spinRequested_ = false;

   float planetSwitchCooldown_ = 0.0f;

   // 惑星切り替え用の変数
   Planet* targetPlanet_ = nullptr;           // 切り替え先の惑星
   GameEngine::Vector3 transitionVelocity_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);  // 遷移中の速度
   float attractionStrength_ = 50.0f;         // 引力の強さ
   bool isTransitioning_ = false;             // 惑星遷移中かどうか
   
   // 回転遷移用の変数
   GameEngine::Quaternion transitionStartRotation_ = GameEngine::Quaternion::Identity();  // 遷移開始時の回転
   GameEngine::Quaternion transitionTargetRotation_ = GameEngine::Quaternion::Identity(); // 遷移目標の回転
   float transitionRotationProgress_ = 0.0f;   // 回転遷移の進行度 (0.0 ~ 1.0)
   float transitionRotationSpeed_ = 10.0f;      // 回転遷移の速度
   
   // スピン関連
   bool isSpinning_ = false;                   // スピン中かどうか
   float spinTimer_ = 0.0f;                    // スピン経過時間
   float spinDuration_ = 0.2f;                 // スピンの持続時間（0.5秒で1回転）
   float spinRotationAngle_ = 0.0f;            // 現在のスピン回転角度
   std::unique_ptr<class SphereCollider> spinCollider_ = nullptr;  // スピン用の当たり判定（通常の1.5倍の半径）
   
   // 全惑星のリスト（最近接惑星検索用）
   std::vector<Planet*> allPlanets_;

   // 現在コリジョン中の惑星リスト（重力判定に触れている惑星）
   std::vector<Planet*> collidingPlanets_;

   // プレイヤー専用ポイントライト
   GameEngine::PointLight* playerPointLight_ = nullptr;

   // コールバック関数
   std::function<void(const GameEngine::Vector3&)> moveCallback_ = nullptr;
   std::function<void()> stopCallback_ = nullptr;
   std::function<void(const GameEngine::Vector3&)> jumpCallback_ = nullptr;
   std::function<void(const GameEngine::Vector3&)> landCallback_ = nullptr;
   std::function<void(class Rabbit*)> rabbitCaptureCallback_ = nullptr;
};