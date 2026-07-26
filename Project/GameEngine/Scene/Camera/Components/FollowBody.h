#pragma once
#include "../Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを追従するBodyコンポーネント
class FollowBody : public ICinemachineComponent {
public:
    /// @brief 既定の追従オフセットと減衰で生成する
    FollowBody() = default;
    /// @brief カメラコンポーネントを破棄する
    ~FollowBody() override = default;

    /// @copydoc ICinemachineComponent::MutateCameraState
    void MutateCameraState(CameraState& state, float deltaTime) override;
    /// @copydoc ICinemachineComponent::GetStage
    CinemachineStage GetStage() const override { return CinemachineStage::Body; }
    /// @copydoc ICinemachineComponent::GetComponentName
    const char* GetComponentName() const override { return "FollowBody"; }

    /// @brief オフセットを設定
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    /// @brief 追従対象からのオフセットを取得する
    const Vector3& GetOffset() const { return offset_; }

    /// @brief ダンピングを設定（各軸ごと）
    void SetDamping(const Vector3& damping) { damping_ = damping; }
    /// @brief 軸ごとの追従ダンピングを取得する
    const Vector3& GetDamping() const { return damping_; }

    /// @copydoc ICinemachineComponent::Serialize
    nlohmann::json Serialize() const override;
    /// @copydoc ICinemachineComponent::Deserialize
    void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
    /// @copydoc ICinemachineComponent::DrawInspector
    void DrawInspector() override;
#endif

private:
    Vector3 offset_ = { 0.0f, 5.0f, -10.0f };
    Vector3 damping_ = { 1.0f, 1.0f, 1.0f };
};

} // namespace GameEngine
