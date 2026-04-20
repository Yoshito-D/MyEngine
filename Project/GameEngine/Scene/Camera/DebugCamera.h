#pragma once
#include "Core/VirtualCamera.h"
#include "Components/OrbitalBody.h"

namespace GameEngine {

class Camera;

/// @brief デバッグ用の周回カメラ（Cinemachineシステム使用）
class DebugCamera : public VirtualCamera {
public:
    /// @brief 初期化
    /// @param initialState 初期カメラ状態
    void Initialize(const CameraState& initialState = CameraState()) override;

    /// @brief デバッグカメラの更新
    /// @param deltaTime フレーム時間
    void Update(float deltaTime) override;

    /// @brief ピボットターゲットとの距離を設定する
    void SetDistance(float distance);

    /// @brief OrbitalBodyコンポーネントを取得
    OrbitalBody* GetOrbitalBody() const { return orbitalBody_; }

private:
    OrbitalBody* orbitalBody_ = nullptr;
};

} // namespace GameEngine