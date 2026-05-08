#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"

namespace GameEngine {

/// @brief カメラの状態を表す値オブジェクト
struct CameraState {
    Transform transform;
    float fov = 0.45f;
    float nearClip = 0.01f;
    float farClip = 100.0f;

    // ビュー行列を直接保持するフラグと行列
    // trueの場合、Transform→行列変換を行わずviewMatrixOverrideを使用する
    bool hasViewMatrixOverride = false;
    Matrix4x4 viewMatrixOverride = MakeIdentity4x4();

    CameraState() {
        transform.SetRotationQuaternion(Quaternion::Identity());
    }

    /// @brief ビュー行列を直接セット（Transform経由の変換を行わない）
    void SetViewMatrix(const Matrix4x4& viewMatrix) {
        viewMatrixOverride = viewMatrix;
        hasViewMatrixOverride = true;
    }

    /// @brief 有効なビュー行列を取得
    Matrix4x4 GetViewMatrix() const {
        if (hasViewMatrixOverride) {
            return viewMatrixOverride;
        }
        return GetWorldMatrix().Inverse();
    }

    /// @brief 2つのカメラ状態を線形補間
    static CameraState Lerp(const CameraState& a, const CameraState& b, float t) {
        CameraState result;

        // ビュー行列オーバーライドが片方または両方ある場合、有効な行列同士を補間する
        if (a.hasViewMatrixOverride || b.hasViewMatrixOverride) {
            Matrix4x4 viewA = a.GetViewMatrix();
            Matrix4x4 viewB = b.GetViewMatrix();
            // 各列を線形補間してビュー行列を合成
            Matrix4x4 blended;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    blended.m[i][j] = viewA.m[i][j] + (viewB.m[i][j] - viewA.m[i][j]) * t;
                }
            }
            result.SetViewMatrix(blended);
        }

        result.transform.translation = Vector3::Lerp(a.transform.translation, b.transform.translation, t);
        result.transform.scale = Vector3::Lerp(a.transform.scale, b.transform.scale, t);
        result.transform.SetRotationQuaternion(
            Quaternion::Slerp(a.transform.GetActiveQuaternion(), b.transform.GetActiveQuaternion(), t)
        );
        result.fov = a.fov + (b.fov - a.fov) * t;
        result.nearClip = a.nearClip + (b.nearClip - a.nearClip) * t;
        result.farClip = a.farClip + (b.farClip - a.farClip) * t;
        return result;
    }

    /// @brief ワールド行列を計算
    Matrix4x4 GetWorldMatrix() const {
        Matrix4x4 scaleMatrix = MakeScaleMatrix(transform.scale);
        Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
        Matrix4x4 translateMatrix = MakeTranslateMatrix(transform.translation);
        return scaleMatrix * rotationMatrix * translateMatrix;
    }

    /// @brief 前方ベクトルを取得
    Vector3 GetForward() const {
        Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
        Vector4 forward = TransformVectorByMatrix({ 0.0f, 0.0f, 1.0f, 1.0f }, rotationMatrix);
        return { forward.x, forward.y, forward.z };
    }

    /// @brief 右ベクトルを取得
    Vector3 GetRight() const {
        Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
        Vector4 right = TransformVectorByMatrix({ 1.0f, 0.0f, 0.0f, 1.0f }, rotationMatrix);
        return { right.x, right.y, right.z };
    }

    /// @brief 上ベクトルを取得
    Vector3 GetUp() const {
        Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
        Vector4 up = TransformVectorByMatrix({ 0.0f, 1.0f, 0.0f, 1.0f }, rotationMatrix);
        return { up.x, up.y, up.z };
    }
};

} // namespace GameEngine
