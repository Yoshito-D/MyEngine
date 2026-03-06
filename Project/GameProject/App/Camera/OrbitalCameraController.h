#pragma once
#include "Scene/Camera/Camera.h"
#include "CameraKeyframe.h"
#include "Utility/VectorMath.h"
#include <functional>

/// @brief 注視点を中心に回転するカメラコントローラー
/// オープニング演出などで使用
class OrbitalCameraController {
public:
   /// @brief 軌道パラメータ
   struct OrbitParams {
	  GameEngine::Vector3 targetPosition;         // 注視点の位置
	  float radius = 10.0f;           // 軌道の半径
	  float startAngleY = 0.0f;       // 開始角度（Y軸周り、ラジアン）
	  float endAngleY = 6.28318f;     // 終了角度（Y軸周り、ラジアン）デフォルトは360度
	  float startAngleX = 0.0f;       // 開始角度（X軸周り、ラジアン）
	  float endAngleX = 0.0f;         // 終了角度（X軸周り、ラジアン）
	  float duration = 5.0f;          // 完了までの時間（秒）
	  CameraKeyframe::EasingType easingType = CameraKeyframe::EasingType::Linear;
	  float easingPower = 2.0f;       // イージングの強さ
	  float fov = 0.45f;              // 視野角
	  bool lookAtTarget = true;       // 常にターゲットを見るか
   };

   /// @brief コンストラクタ
   /// @param camera 制御するカメラ
   OrbitalCameraController(GameEngine::Camera* camera);
   ~OrbitalCameraController() = default;

   /// @brief 軌道パラメータを設定
   /// @param params 軌道パラメータ
   void SetOrbitParams(const OrbitParams& params);

   /// @brief 軌道移動を開始
   /// @param loop ループするか
   void Start(bool loop = false);

   /// @brief 軌道移動を停止
   void Stop();

   /// @brief 一時停止
   void Pause();

   /// @brief 再開
   void Resume();

   /// @brief リセット（最初に戻る）
   void Reset();

   /// @brief 更新（毎フレーム呼ぶ）
   void Update();

   /// @brief 移動中か
   /// @return 移動中の場合true
   bool IsMoving() const { return isMoving_; }

   /// @brief 完了したか
   /// @return 完了した場合true
   bool IsCompleted() const { return isCompleted_; }

   /// @brief 現在の進行度を取得（0.0～1.0）
   /// @return 進行度
   float GetProgress() const;

   /// @brief 完了時のコールバックを設定
   /// @param callback コールバック関数
   void SetOnCompleteCallback(std::function<void()> callback) { onCompleteCallback_ = callback; }

   /// @brief 現在の位置を取得
   /// @return カメラ位置
   GameEngine::Vector3 GetCurrentPosition() const { return currentPosition_; }

   /// @brief 現在の角度を取得（Y軸周り）
   /// @return 角度（ラジアン）
   float GetCurrentAngleY() const { return currentAngleY_; }

   /// @brief 現在の角度を取得（X軸周り）
   /// @return 角度（ラジアン）
   float GetCurrentAngleX() const { return currentAngleX_; }
   
   /// @brief 現在のradiusを動的に設定（演出中に使用）
   /// @param radius 新しい半径
   void SetCurrentRadius(float radius) { currentRadius_ = radius; }
   
   /// @brief 現在のradiusを取得
   /// @return 現在の半径
   float GetCurrentRadius() const { return currentRadius_; }

private:
   /// @brief 球面座標から直交座標に変換
   /// @param radius 半径
   /// @param angleY Y軸周りの角度（ラジアン）
   /// @param angleX X軸周りの角度（ラジアン）
   /// @param center 中心位置
   /// @return 直交座標
   GameEngine::Vector3 SphericalToCartesian(float radius, float angleY, float angleX, const GameEngine::Vector3& center);

   /// @brief ターゲットを向く回転を計算
   /// @param position カメラ位置
   /// @param target ターゲット位置
   /// @return 回転（Euler角）
   GameEngine::Vector3 CalculateLookAtRotation(const GameEngine::Vector3& position, const GameEngine::Vector3& target);

   GameEngine::Camera* camera_ = nullptr;
   OrbitParams params_;
   float currentTime_ = 0.0f;
   float currentAngleY_ = 0.0f;
   float currentAngleX_ = 0.0f;
   float currentRadius_ = 10.0f;  // 現在の半径（動的に変更可能）
   GameEngine::Vector3 currentPosition_;
   bool isMoving_ = false;
   bool isPaused_ = false;
   bool isLooping_ = false;
   bool isCompleted_ = false;

   std::function<void()> onCompleteCallback_ = nullptr;
};
