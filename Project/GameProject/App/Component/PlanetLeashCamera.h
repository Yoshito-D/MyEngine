#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace GameEngine {

/// @brief 惑星対応レアッシュカメラコンポーネント
///
/// プレイヤーを常に注視し続け、プレイヤーから一定距離(maxFollowDistance)以上
/// 離れると追従して近づく。また、惑星中心から最小距離(minPlanetDistance)を
/// 保持することで惑星表面にめり込まない。
///
/// - 注視: 毎フレーム pivotTarget_ を向き続ける（LookAt）
/// - レアッシュ: カメラとpivotTargetの距離 > maxFollowDistance なら近づく
/// - 惑星クランプ: eye と sphereCenter_ の距離 < minPlanetDistance なら押し出す
class PlanetLeashCamera : public ICinemachineComponent {
public:
    PlanetLeashCamera() = default;
    ~PlanetLeashCamera() override = default;

    void MutateCameraState(CameraState& state, float deltaTime) override;
    CinemachineStage GetStage() const override { return CinemachineStage::Body; }

    // --- 外部から毎フレーム設定するパラメータ ---

    /// @brief 追従するターゲット位置（プレイヤー位置）を設定
    void SetPivotTarget(const Vector3& target) { pivotTarget_ = target; }

    /// @brief 惑星中心座標を設定（最小距離クランプ用）
    void SetSphereCenter(const Vector3& center) { sphereCenter_ = center; }

    /// @brief カメラの初期位置を直接設定（シーン開始時に一度呼ぶ）
    void SetInitialEyePosition(const Vector3& eye) {
        eyePos_ = eye;
        isInitialized_ = true;
    }

    // --- パラメータ ---

    /// @brief この距離を超えるとカメラが追従を開始する
    float maxFollowDistance = 8.0f;

    /// @brief 追従時の移動速度（units/sec）
    float followSpeed = 6.0f;

    /// @brief 惑星中心からこの距離より近づかない（惑星半径 + マージン）
    float minPlanetDistance = 7.0f;

    /// @brief 注視するupベクトルを重力upに追従させるか
    /// true = gravityUp を使用、false = ワールドY
    bool useGravityUp = true;

    /// @brief 重力Upベクトル（useGravityUp=true のとき参照）
    void SetGravityUp(const Vector3& up) { gravityUp_ = up; }

    /// @brief スクリーンの上ベクトルを返す（PlayerControllerのスクリーンスペース投影用）
    Vector3 GetCameraUp() const { return cachedUp_; }

    /// @brief スクリーンの右ベクトルを返す（PlayerControllerのスクリーンスペース投影用）
    Vector3 GetCameraRight() const { return cachedRight_; }

private:
    Vector3 pivotTarget_  = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereCenter_ = { 0.0f, 0.0f, 0.0f };
    Vector3 gravityUp_    = { 0.0f, 1.0f, 0.0f };

    Vector3 eyePos_        = { 0.0f, 5.0f, -15.0f }; ///< カメラのワールド座標（毎フレーム更新）
    bool    isInitialized_ = false;
    Vector3 prevGravityUp_ = { 0.0f, 1.0f, 0.0f };  ///< 前フレームのgravityUp（ロール補正用）
    Vector3 eyeRelUp_      = { 0.0f, 1.0f, 0.0f };  ///< カメラのローカルUp（LookAt用・連続更新）

    // MutateCameraStateで更新されるスクリーン軸キャッシュ
    mutable Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };
    mutable Vector3 cachedUp_    = { 0.0f, 1.0f, 0.0f };
};

} // namespace GameEngine
