#pragma once
#include "../Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを注視するAimコンポーネント
class LookAtAim : public ICinemachineComponent {
public:
    /// @brief 既定の注視オフセットと減衰で生成する
    LookAtAim() = default;
    /// @brief カメラコンポーネントを破棄する
    ~LookAtAim() override = default;

    /// @copydoc ICinemachineComponent::MutateCameraState
    void MutateCameraState(CameraState& state, float deltaTime) override;
    /// @copydoc ICinemachineComponent::GetStage
    CinemachineStage GetStage() const override { return CinemachineStage::Aim; }
    /// @copydoc ICinemachineComponent::GetComponentName
    const char* GetComponentName() const override { return "LookAtAim"; }

    /// @brief ダンピングを設定
    void SetDamping(float damping) { damping_ = damping; }
    /// @brief 注視回転のダンピングを取得する
    float GetDamping() const { return damping_; }

    /// @brief オフセットを設定（注視点のオフセット）
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    /// @brief 注視点のオフセットを取得する
    const Vector3& GetOffset() const { return offset_; }

    /// @copydoc ICinemachineComponent::Serialize
    nlohmann::json Serialize() const override;
    /// @copydoc ICinemachineComponent::Deserialize
    void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
    /// @copydoc ICinemachineComponent::DrawInspector
    void DrawInspector() override;
#endif

private:
    float damping_ = 10.0f;
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
};

} // namespace GameEngine
