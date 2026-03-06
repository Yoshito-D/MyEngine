#pragma once
#include "Camera/Camera.h"
#include "Utility/VectorMath.h"
#include "Core/Input/KeyConfig.h"
#include <memory>

// 前方宣言
class Player;
class Planet;

/// @brief TPS（三人稱視点）カメラコントローラー
/// プレイヤーの頭上に配置し、真下を見下ろす
/// デバッグカメラと同じ設計でビュー行列を直接作成
class TPSCameraController {
public:
   TPSCameraController();
   
   /// @brief 初期化
   /// @param camera 制御するカメラ
   /// @param target 追従対象のプレイヤー
   void Initialize(GameEngine::Camera* camera, Player* target);
   
   /// @brief 更新
   void Update();
   
   /// @brief カメラの距離を設定
   void SetDistance(float distance) { distance_ = distance; }
   
   /// @brief ロールの補間速度を設定
   void SetRollSpeed(float speed) { rollSpeed_ = speed; }
   
   /// @brief カメラの回転速度を設定
   void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }
   
   /// @brief カメラの前方向を取得（プレイヤーへの視線方向の逆）
   GameEngine::Vector3 GetCameraForward() const;
   
   /// @brief カメラの右方向を取得
   GameEngine::Vector3 GetCameraRight() const;
   
   /// @brief 惑星リストを設定（衝突判定用）
   void SetPlanets(const std::vector<Planet*>& planets) { planets_ = planets; }

private:
   GameEngine::Camera* camera_ = nullptr;
   Player* target_ = nullptr;
   
   // キーコンフィグ
   std::unique_ptr<GameEngine::KeyConfig> keyConfig_ = nullptr;
   
   // カメラパラメータ（プレイヤーのローカル座標系での相対回転）
   float localYaw_ = 0.0f;            // プレイヤーの横軸周りの回転
   float localPitch_ = 0.0f;          // プレイヤーの縦軸周りの回転
   float roll_ = 0.0f;                // ロール角（Z軸回転、プレイヤーの傾きに追従）
   float distance_ = 25.0f;           // プレイヤーからの距離
   float rollSpeed_ = 10.0f;          // ロールの補間速度
   float rotateSpeed_ = 0.05f;        // カメラの回転速度
   
   GameEngine::Vector3 pivotTarget_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);  // 注視点（プレイヤー位置）
   GameEngine::Matrix4x4 rotationMatrix_;         // 回転行列
   
   // カメラの方向ベクトル（キャッシュ）
   GameEngine::Vector3 cameraForward_ = GameEngine::Vector3(0.0f, 0.0f, 1.0f);
   GameEngine::Vector3 cameraRight_ = GameEngine::Vector3(1.0f, 0.0f, 0.0f);
   
   // カメラ位置の補間用
   GameEngine::Vector3 currentEye_ = GameEngine::Vector3(0.0f, 0.0f, 0.0f);   // 現在のカメラ位置
   float followSpeed_ = 3.0f;          // カメラの追従速度
   
   // 惑星切り替え検知用
   Planet* previousPlanet_ = nullptr;
   GameEngine::Quaternion previousPlayerQuat_ = GameEngine::Quaternion::Identity();
   
   // カメラ回転の補間用
   GameEngine::Quaternion targetCameraRotation_ = GameEngine::Quaternion::Identity();
   GameEngine::Quaternion currentCameraRotation_ = GameEngine::Quaternion::Identity();
   float cameraRotationSpeed_ = 10.0f;  // カメラ回転の補間速度
   
   // キー入力検知用
   bool isManualRotating_ = false;      // キー入力でカメラを回転中かどうか
   
   // 惑星切り替え時の滑らかな遷移用
   bool isPlanetTransitioning_ = false;  // 惑星切り替え中かどうか
   float transitionTimer_ = 0.0f;        // 遷移タイマー
   float transitionDuration_ = 0.0f;     // 遷移時間
   float savedDistance_ = 25.0f;         // 惑星切り替え前の距離を保存
   
   // ロール遷移用
   float startRoll_ = 0.0f;              // 遷移開始時のロール角
   float targetRollForTransition_ = 0.0f; // 遷移先のロール角
   bool isRollTransitioning_ = false;    // ロール遷移中かどうか
   float rollTransitionTimer_ = 0.0f;    // ロール遷移タイマー
   float rollTransitionDuration_ = 0.0f; // ロール遷移時間（より長く、滑らかに）
   
   // 衝突判定用
   std::vector<Planet*> planets_;       // 惑星リスト
   float cameraMinDistance_ = 1.0f;     // カメラの最小距離（惑星表面からのマージン）
   
   /// @brief カメラと惑星の衝突判定を行い、カメラ位置を調整
   GameEngine::Vector3 ResolveCollisionWithPlanets(const GameEngine::Vector3& desiredEye, const GameEngine::Vector3& pivot);
};
