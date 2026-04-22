#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace GameEngine {

/// @brief 重力追従型OrbitalCameraコンポーネント（フェーズ4）
///
/// プレイヤーをpivotTargetとして追従し、カメラのUpVectorを
/// 重力の「上」方向（惑星中心から外向き）に自動補正するBodyコンポーネント。
///
/// OrbitalBodyとの違い:
///   - Yaw/Pitchの回転基底をワールドYではなくgravityUp軸で構築する
///   - これにより惑星のどこにいてもカメラが「足元基準」で正しく機能する
///
/// アルゴリズム:
///   1. gravityUp を基準にright = gravityUp × worldFwd を構築
///   2. yaw回転: gravityUp軸周りに pivot する
///   3. pitch回転: right軸周りに上下する
///   4. eye = pivotTarget + offset, LookAt(eye, pivot, gravityUp) でビュー行列生成
class GravityFollowCamera : public ICinemachineComponent {
public:
   GravityFollowCamera() = default;
   ~GravityFollowCamera() override = default;

   void MutateCameraState(CameraState& state, float deltaTime) override;
   CinemachineStage GetStage() const override { return CinemachineStage::Body; }

   /// @brief マウス/スティック入力を処理してYaw/Pitchを更新
   void ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging);

   // --- 外部から毎フレーム設定するパラメータ ---

   /// @brief 重力Upベクトルを設定（惑星中心→プレイヤー方向の正規化ベクトル）
   void SetGravityUp(const Vector3& up) { gravityUp_ = up; }

   /// @brief 追従するピボット位置（プレイヤー位置）を設定
   void SetPivotTarget(const Vector3& target) { pivotTarget_ = target; }

   // --- ゲッター ---
   Vector3 GetGravityUp() const { return gravityUp_; }
   const Vector3& GetPivotTarget() const { return pivotTarget_; }
   float GetPitch() const { return pitch_; }
   float GetDistance() const { return distance_; }
   void SetDistance(float d) { distance_ = (std::max)(0.5f, d); }
   /// @brief スクリーンの上ベクトルを返す（PlayerControllerのスクリーンスペース投影用）
   Vector3 GetCameraUp() const;

   /// @brief スクリーンの右ベクトルを返す（PlayerControllerのスクリーンスペース投影用）
   Vector3 GetCameraRight() const;

public:
   float rotateSpeed = 0.005f;     ///< 回転速度
   float scrollSpeed = 1.0f / 120.0f; ///< ズーム速度

private:
   float pitch_ = 1.0f;         ///< 垂直回転角（正 = 上から見下ろす）
   float distance_ = 10.0f;        ///< ピボットからの距離

   // 赤道問題の根本修正:
   // 絶対Yaw角+固定基準ベクトルの代わりに「重力平面上のカメラ前方」を保存する。
   // 毎フレーム gravityUp 平面に再投影することで、どの緯度でも連続した動作を保証。
   Vector3 flatForward_ = { 0.0f, 0.0f, 1.0f }; ///< gravityUp平面上のカメラ前方（正規化済み）

   Vector3 gravityUp_ = { 0.0f, 1.0f, 0.0f }; ///< 重力Up（毎フレーム外部から更新）
   Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f }; ///< 追従ターゲット位置

   // カメラのローカル軸（MutateCameraStateで更新され、GetCameraUp/Rightで参照）
   mutable Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };
   mutable Vector3 cachedUp_ = { 0.0f, 1.0f, 0.0f };
};

} // namespace GameEngine
