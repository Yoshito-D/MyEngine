#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace App {

/// @brief 重力Upを基準に旋回・ズームする追従カメラコンポーネント
class GravityFollowCamera : public GameEngine::ICinemachineComponent {
public:
   GravityFollowCamera() = default;
   ~GravityFollowCamera() override = default;

   /// @brief カメラ状態を更新する
   /// @param state 更新対象のカメラ状態
   /// @param deltaTime フレーム時間（現在は未使用）
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

   /// @brief 実行ステージ（Body）を返す
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

   /// @brief コンポーネント名を返す
   const char* GetComponentName() const override { return "GravityFollowCamera"; }

   /// @brief 入力を反映して方位・ピッチ・距離を更新する
   void ProcessInput(const GameEngine::Vector2& mouseDelta, int32_t wheelDelta, bool isDragging);

   /// @brief 重力Upを設定する
   void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

   /// @brief 注視対象（ピボット）を設定する
   void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

   /// @brief 現在の重力Upを取得する
   GameEngine::Vector3 GetGravityUp() const { return gravityUp_; }

   /// @brief 注視対象（ピボット）を取得する
   const GameEngine::Vector3& GetPivotTarget() const { return pivotTarget_; }

   /// @brief 現在のピッチ角（ラジアン）を取得する
   float GetPitch()    const { return pitch_; }

   /// @brief 現在のカメラ距離を取得する
   float GetDistance() const { return distance_; }

   /// @brief カメラ距離を設定する（下限あり）
   void  SetDistance(float d) { distance_ = (std::max)(0.5f, d); }

   /// @brief 直近更新時のカメラUpを取得する
   GameEngine::Vector3 GetCameraUp()    const;

   /// @brief 直近更新時のカメラRightを取得する
   GameEngine::Vector3 GetCameraRight() const;

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

public:
   /// @brief 入力回転感度
   float rotateSpeed = 0.005f;

   /// @brief ホイールズーム感度
   float scrollSpeed = 1.0f / 120.0f;

private:
   /// @brief ピッチ角（ラジアン）
   float pitch_ = 1.0f;

   /// @brief ピボットからのカメラ距離
   float distance_ = 10.0f;

   /// @brief 重力平面上の基準前方ベクトル
   GameEngine::Vector3 flatForward_ = { 0.0f, 0.0f, 1.0f };

   /// @brief 外部から供給される重力Up
   GameEngine::Vector3 gravityUp_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 注視対象座標
   GameEngine::Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 直近計算のカメラRight
   mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };

   /// @brief 直近計算のカメラUp
   mutable GameEngine::Vector3 cachedUp_ = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
