#pragma once
#include "../Core/ICinemachineComponent.h"
#include <algorithm>

namespace GameEngine {

/// @brief ターゲットの周りを周回するBodyコンポーネント
/// DebugCameraの動作を再現
class OrbitalBody : public ICinemachineComponent {
public:
    OrbitalBody() = default;
    ~OrbitalBody() override = default;

    void MutateCameraState(CameraState& state, float deltaTime) override;
    CinemachineStage GetStage() const override { return CinemachineStage::Body; }

    /// @brief 入力処理
    /// @param mouseDelta マウス移動量
    /// @param wheelDelta マウスホイール量
    /// @param isDragging ドラッグ中かどうか
    /// @param isShiftPressed シフトキーが押されているか
    void ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging, bool isShiftPressed);

    /// @brief 距離を設定
    void SetDistance(float distance) { distance_ = (std::max)(0.5f, distance); }
    float GetDistance() const { return distance_; }

    /// @brief ピボットターゲットを設定
    void SetPivotTarget(const Vector3& target) { pivotTarget_ = target; }
    const Vector3& GetPivotTarget() const { return pivotTarget_; }

    /// @brief 回転速度を設定
    void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }
    /// @brief スクロール速度を設定
    void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }
    /// @brief 移動速度を設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    /// @brief Yaw角度を取得
    float GetYaw() const { return yaw_; }
    /// @brief Pitch角度を取得
    float GetPitch() const { return pitch_; }
    /// @brief Yaw角度を設定
    void SetYaw(float yaw) { yaw_ = yaw; }
    /// @brief Pitch角度を設定
    void SetPitch(float pitch) { pitch_ = pitch; }

    /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの前方ベクトルを返す
    Vector3 GetCameraForward() const {
        float cosP = std::cos(pitch_);
        float sinP = std::sin(pitch_);
        float cosY = std::cos(yaw_);
        float sinY = std::sin(yaw_);
        Vector3 offset = { cosP * sinY, -sinP, cosP * cosY };
        return (-offset).Normalize();
    }

    /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの上ベクトルを返す（スクリーン上方向）
    Vector3 GetCameraUp() const {
        float cosP = std::cos(pitch_);
        float sinP = std::sin(pitch_);
        float cosY = std::cos(yaw_);
        float sinY = std::sin(yaw_);
        // rotationMatrix_ = Rx(pitch)*Ry(yaw) の行1 = TransformNormal({0,1,0}, matrix)
        return { sinP * sinY, cosP, sinP * cosY };
    }

    /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの右ベクトルを返す（スクリーン右方向）
    Vector3 GetCameraRight() const {
        float cosY = std::cos(yaw_);
        float sinY = std::sin(yaw_);
        // LookAtMatrix の xaxis = up.Cross(zaxis) の解析解
        return { -cosY, 0.0f, sinY };
    }

    const char* GetComponentName() const override { return "OrbitalBody"; }

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

private:
    float yaw_ = 3.14159f;  // PI
    float pitch_ = -0.785f; // -45度
    float distance_ = 25.0f;
    Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f };

    float rotateSpeed_ = 0.01f;
    float scrollSpeed_ = 1.0f / 120.0f;
    float moveSpeed_ = 0.0008f;

    Matrix4x4 rotationMatrix_ = MakeIdentity4x4();
};

} // namespace GameEngine
