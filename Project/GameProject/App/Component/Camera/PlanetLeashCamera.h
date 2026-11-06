#pragma once

#include <cmath>
#include <algorithm>
#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"
#include "GameProject/App/Component/Camera/IVehicleCamera.h"
#include <algorithm>

namespace App {

/// @brief 惑星表面周辺での追従距離と最小半径を保つレアッシュカメラ
class PlanetLeashCamera : public GameEngine::ICinemachineComponent, public IVehicleCamera {
public:
   /// @brief レアッシュカメラを既定の追従設定で生成する
   PlanetLeashCamera() = default;

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
   void SetInitialEyePosition(const GameEngine::Vector3& eye);

   /// @brief 重力Upを設定する
   void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

   /// @brief 共通追従入力を、このカメラの位置・重力・速度入力へ適用する。
   void ApplyVehicleFollowInput(const VehicleCameraFollowInput& input) override;

   /// @brief 保存設定を変更せず、距離倍率と高さ加算（m）の一時演出を適用する。
   void ApplyVehicleEffectInput(const VehicleCameraEffectInput& input) override;

   /// @brief 選択確認とシェイク通知に使用するカメラ部品を返す。
   GameEngine::ICinemachineComponent& GetVehicleCameraComponent() override { return *this; }

   /// @brief 直近のカメラUp方向を返す。
   GameEngine::Vector3 GetVehicleCameraUp() const override { return GetCameraUp(); }

   /// @brief 直近計算のカメラUpを取得する
   GameEngine::Vector3 GetCameraUp() const { return cachedUp_; }

   /// @brief 直近計算のカメラRightを取得する
   GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

   /// @brief 惑星追従カメラの保存対象設定をJSONへ変換する。
   /// @return 追従距離・惑星との最小距離・追従速度とUpの選択条件を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONから惑星追従カメラの設定を復元する。
   /// @param data 追従距離・惑星との最小距離・追従速度とUpの選択条件を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 惑星追従カメラの共有可能な調整値をJSONで取得する。
   /// @return 追従距離・惑星との最小距離・追従速度とUpの選択条件を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 惑星追従カメラの共有設定を既存コンポーネントへ反映する。
   /// @param data 追従距離・惑星との最小距離・追従速度とUpの選択条件を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 追従開始距離（この距離を超えた分を追う）
      float maxFollowDistance = 8.0f;

      /// @brief 追従速度
      float followSpeed = 6.0f;

      /// @brief 惑星中心からの最小許容距離
      float minPlanetDistance = 7.0f;

      /// @brief 重力Up使用フラグ（将来拡張向け）
      bool useGravityUp = true;
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float maxFollowDistance = 8.0f; ///< 追従開始距離（この距離を超えた分を追う）。
   float followSpeed = 6.0f;       ///< 追従速度。
   float minPlanetDistance = 7.0f; ///< 惑星中心からの最小許容距離。
   bool useGravityUp = true;       ///< 重力Up使用フラグ（将来拡張向け）。

private:
   /// @brief 注視対象座標
   GameEngine::Vector3 pivotTarget_ = {0.0f, 0.0f, 0.0f};

   /// @brief 惑星中心座標
   GameEngine::Vector3 sphereCenter_ = {0.0f, 0.0f, 0.0f};

   /// @brief 外部から供給される重力Up
   GameEngine::Vector3 gravityUp_ = {0.0f, 1.0f, 0.0f};

   /// @brief カメラの現在位置
   GameEngine::Vector3 eyePos_ = {0.0f, 5.0f, -15.0f};

   /// @brief レアッシュの通常位置から表示位置だけを近づける距離倍率
   float driftDistanceScale_ = 1.0f;

   /// @brief 通常の表示位置へ加えるドリフト演出のカメラ高さ
   float driftCameraHeightOffset_ = 0.0f;

   /// @brief 通常の注視点へ加えるドリフト演出の高さ
   float driftLookTargetHeightOffset_ = 0.0f;

   /// @brief 初期化実施済みフラグ
   bool isInitialized_ = false;

   /// @brief 前フレームの重力Up
   GameEngine::Vector3 prevGravityUp_ = {0.0f, 1.0f, 0.0f};

   /// @brief カメラ相対Up（ロール安定用）
   GameEngine::Vector3 eyeRelUp_ = {0.0f, 1.0f, 0.0f};

   /// @brief 直近計算のカメラRight
   mutable GameEngine::Vector3 cachedRight_ = {1.0f, 0.0f, 0.0f};

   /// @brief 直近計算のカメラUp
   mutable GameEngine::Vector3 cachedUp_ = {0.0f, 1.0f, 0.0f};
};

} // namespace App
