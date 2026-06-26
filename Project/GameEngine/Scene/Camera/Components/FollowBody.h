#pragma once
#include "../Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを追従するBodyコンポーネント
class FollowBody : public ICinemachineComponent {
public:
    FollowBody() = default;
    ~FollowBody() override = default;

    void MutateCameraState(CameraState& state, float deltaTime) override;
    CinemachineStage GetStage() const override { return CinemachineStage::Body; }
    const char* GetComponentName() const override { return "FollowBody"; }

    /// @brief オフセットを設定
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    const Vector3& GetOffset() const { return offset_; }

    /// @brief ダンピングを設定（各軸ごと）
    void SetDamping(const Vector3& damping) { damping_ = damping; }
    const Vector3& GetDamping() const { return damping_; }

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

private:
    Vector3 offset_ = { 0.0f, 5.0f, -10.0f };
    Vector3 damping_ = { 1.0f, 1.0f, 1.0f };
};

} // namespace GameEngine
