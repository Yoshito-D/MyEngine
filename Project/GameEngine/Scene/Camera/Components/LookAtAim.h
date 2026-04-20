#pragma once
#include "../Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを注視するAimコンポーネント
class LookAtAim : public ICinemachineComponent {
public:
    LookAtAim() = default;
    ~LookAtAim() override = default;

    void MutateCameraState(CameraState& state, float deltaTime) override;
    CinemachineStage GetStage() const override { return CinemachineStage::Aim; }

    /// @brief ダンピングを設定
    void SetDamping(float damping) { damping_ = damping; }
    float GetDamping() const { return damping_; }

    /// @brief オフセットを設定（注視点のオフセット）
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    const Vector3& GetOffset() const { return offset_; }

private:
    float damping_ = 10.0f;
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
};

} // namespace GameEngine
