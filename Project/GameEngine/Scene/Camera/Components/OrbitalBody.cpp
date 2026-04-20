#include "pch.h"
#include "OrbitalBody.h"
#include "../Core/VirtualCamera.h"
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <algorithm>

namespace GameEngine {

void OrbitalBody::MutateCameraState(CameraState& state, float /*deltaTime*/) {
    rotationMatrix_ = MakeRotateXMatrix(pitch_) * MakeRotateYMatrix(yaw_);

    Vector3 offset = TransformCoordinate({ 0.0f, 0.0f, distance_ }, rotationMatrix_);
    Vector3 eye = pivotTarget_ + offset;
    Vector3 up = TransformNormal({ 0.0f, 1.0f, 0.0f }, rotationMatrix_);

    // 元のDebugCameraと完全に同じ計算でビュー行列を直接生成する
    // Transform→Quaternion→再構築の変換パスを使わない
    state.transform.translation = eye;
    state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, up));
}

void OrbitalBody::ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta,
                                bool isDragging, bool isShiftPressed) {
    if (isDragging) {
        if (isShiftPressed) {
            // パン操作
            const float cosPitch = std::cos(pitch_);
            const float sinPitch = std::sin(pitch_);
            const float cosYaw = std::cos(yaw_);
            const float sinYaw = std::sin(yaw_);

            Vector3 offset = {
                distance_ * cosPitch * sinYaw,
                -distance_ * sinPitch,
                distance_ * cosPitch * cosYaw
            };

            Vector3 forward = (-offset).Normalize();
            Vector3 right = Vector3(0.0f, 1.0f, 0.0f).Cross(forward);
            if (right.LengthSquared() > 1e-6f) {
                right = right.Normalize();
            } else {
                right = { 1.0f, 0.0f, 0.0f };
            }
            Vector3 up = forward.Cross(right).Normalize();

            float actualMoveSpeed = moveSpeed_ * distance_;

            pivotTarget_ = pivotTarget_ + right * (mouseDelta.x * actualMoveSpeed);
            pivotTarget_ = pivotTarget_ + up * (mouseDelta.y * actualMoveSpeed);
        } else {
            // 回転操作
            yaw_ += mouseDelta.x * rotateSpeed_;
            pitch_ -= mouseDelta.y * rotateSpeed_;

            // ピッチの制限（真上・真下を防ぐ）
            constexpr float kPitchLimit = 1.5f; // 約86度
            pitch_ = std::clamp(pitch_, -kPitchLimit, kPitchLimit);
        }
    }

    // ズーム操作
    if (wheelDelta != 0) {
        distance_ -= wheelDelta * scrollSpeed_;
        distance_ = (std::max)(0.5f, distance_);
    }
}

} // namespace GameEngine
