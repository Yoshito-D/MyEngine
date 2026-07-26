#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace App {

/// @brief 惑星表面周辺での追従距離と最小半径を保つレアッシュカメラ
class PlanetLeashCamera : public GameEngine::ICinemachineComponent {
public:
   /// @brief レアッシュカメラを既定の追従設定で生成する
   PlanetLeashCamera()  = default;
   /// @brief カメラコンポーネントを破棄する
   ~PlanetLeashCamera() override = default;

   /// @brief カメラ状態を更新する
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

   /// @brief 実行ステージ（Body）を返す
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

   /// @brief コンポーネント名を返す
   const char* GetComponentName() const override { return "PlanetLeashCamera"; }

   /// @brief 注視対象（ピボット）を設定する
   void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

   /// @brief 惑星中心を設定する
   void SetSphereCenter(const GameEngine::Vector3& center) { sphereCenter_ = center; }

   /// @brief 初期カメラ位置を設定し、初期化済みにする
   void SetInitialEyePosition(const GameEngine::Vector3& eye) { eyePos_ = eye; isInitialized_ = true; }

   /// @brief 重力Upを設定する
   void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

   /// @brief 直近計算のカメラUpを取得する
   GameEngine::Vector3 GetCameraUp()    const { return cachedUp_; }

   /// @brief 直近計算のカメラRightを取得する
   GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

   /// @copydoc GameEngine::ICinemachineComponent::Serialize
   nlohmann::json Serialize() const override;
   /// @copydoc GameEngine::ICinemachineComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

public:
   /// @brief 追従開始距離（この距離を超えた分を追う）
   float maxFollowDistance  = 8.0f;

   /// @brief 追従速度
   float followSpeed        = 6.0f;

   /// @brief 惑星中心からの最小許容距離
   float minPlanetDistance  = 7.0f;

   /// @brief 重力Up使用フラグ（将来拡張向け）
   bool  useGravityUp       = true;

private:
   /// @brief 注視対象座標
   GameEngine::Vector3 pivotTarget_  = { 0.0f, 0.0f, 0.0f };

   /// @brief 惑星中心座標
   GameEngine::Vector3 sphereCenter_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 外部から供給される重力Up
   GameEngine::Vector3 gravityUp_    = { 0.0f, 1.0f, 0.0f };

   /// @brief カメラの現在位置
   GameEngine::Vector3 eyePos_        = { 0.0f, 5.0f, -15.0f };

   /// @brief 初期化実施済みフラグ
   bool                isInitialized_ = false;

   /// @brief 前フレームの重力Up
   GameEngine::Vector3 prevGravityUp_ = { 0.0f, 1.0f, 0.0f };

   /// @brief カメラ相対Up（ロール安定用）
   GameEngine::Vector3 eyeRelUp_      = { 0.0f, 1.0f, 0.0f };

   /// @brief 直近計算のカメラRight
   mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };

   /// @brief 直近計算のカメラUp
   mutable GameEngine::Vector3 cachedUp_    = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
