#include "pch.h"

#ifdef USE_IMGUI

#include "CameraGizmo.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Core/Renderer/Pass/LineRenderer.h"
#include "Utility/MathUtils.h"
#include <cmath>

namespace GameEngine {

void CameraGizmo::Initialize(LineRenderer* lineRenderer) {
    lineRenderer_ = lineRenderer;
}

void CameraGizmo::DrawFrustum(Camera* camera, Camera* viewCamera) {
    if (!camera || !lineRenderer_) return;

    const Transform& transform = camera->GetTransform();

    CalculateFrustumCorners(
        transform.translation,
        transform.GetActiveQuaternion(),
        camera->GetFovY(),
        camera->GetAspectRatio(),
        camera->GetNearClip(),
        camera->GetFarClip() * settings_.frustumScale,
        nullptr
    );

    // CameraStateを作成して共通処理を呼び出す
    CameraState state;
    state.transform = transform;
    state.fov = camera->GetFovY();
    state.nearClip = camera->GetNearClip();
    state.farClip = camera->GetFarClip();

    DrawFrustum(state, viewCamera);
}

void CameraGizmo::DrawFrustum(const CameraState& state, Camera* viewCamera) {
    if (!lineRenderer_ || !viewCamera) return;

    // アスペクト比を計算（デフォルト16:9）
    float aspectRatio = 16.0f / 9.0f;

    // 視錐台の8頂点を計算
    Vector3 corners[8];
    CalculateFrustumCorners(
        state.transform.translation,
        state.transform.GetActiveQuaternion(),
        state.fov,
        aspectRatio,
        state.nearClip,
        state.farClip * settings_.frustumScale,
        corners
    );

    // 視錐台のエッジを描画
    if (settings_.showFrustum) {
        // Near plane edges
        if (settings_.showNearPlane) {
            DrawQuad(corners[0], corners[1], corners[2], corners[3], settings_.nearPlaneColor, viewCamera);
        }

        // Far plane edges
        if (settings_.showFarPlane) {
            DrawQuad(corners[4], corners[5], corners[6], corners[7], settings_.farPlaneColor, viewCamera);
        }

        // Connecting edges (near to far)
        lineRenderer_->DrawLine(corners[0], corners[4], settings_.frustumColor, viewCamera);
        lineRenderer_->DrawLine(corners[1], corners[5], settings_.frustumColor, viewCamera);
        lineRenderer_->DrawLine(corners[2], corners[6], settings_.frustumColor, viewCamera);
        lineRenderer_->DrawLine(corners[3], corners[7], settings_.frustumColor, viewCamera);
    }

    // カメラの向きを描画
    if (settings_.showDirection) {
        Vector3 forward = state.GetForward();
        Vector3 dirEnd = state.transform.translation + forward * 2.0f;
        lineRenderer_->DrawLine(state.transform.translation, dirEnd, settings_.directionColor, viewCamera);
    }

    // 上方向ベクトルを描画
    if (settings_.showUpVector) {
        Vector3 up = state.GetUp();
        Vector3 upEnd = state.transform.translation + up * 1.0f;
        lineRenderer_->DrawLine(state.transform.translation, upEnd, settings_.upVectorColor, viewCamera);
    }
}

void CameraGizmo::DrawFrustum(VirtualCamera* vcam, Camera* viewCamera) {
    if (!vcam) return;
    DrawFrustum(vcam->GetState(), viewCamera);
}

void CameraGizmo::DrawCameraIcon(const Vector3& position, const Quaternion& rotation, float scale, Camera* viewCamera) {
    if (!lineRenderer_ || !viewCamera) return;

    Matrix4x4 rotMatrix = MakeRotateMatrix(rotation);

    // カメラの向き
    Vector3 forward = { rotMatrix.m[2][0], rotMatrix.m[2][1], rotMatrix.m[2][2] };
    Vector3 right = { rotMatrix.m[0][0], rotMatrix.m[0][1], rotMatrix.m[0][2] };
    Vector3 up = { rotMatrix.m[1][0], rotMatrix.m[1][1], rotMatrix.m[1][2] };

    // カメラボディ（簡易的な箱型）
    float bodySize = scale * 0.5f;
    float lensLength = scale * 0.8f;

    Vector3 bodyCorners[8];
    // 後面
    bodyCorners[0] = position - forward * bodySize - right * bodySize - up * bodySize;
    bodyCorners[1] = position - forward * bodySize + right * bodySize - up * bodySize;
    bodyCorners[2] = position - forward * bodySize + right * bodySize + up * bodySize;
    bodyCorners[3] = position - forward * bodySize - right * bodySize + up * bodySize;
    // 前面
    bodyCorners[4] = position + forward * bodySize * 0.3f - right * bodySize - up * bodySize;
    bodyCorners[5] = position + forward * bodySize * 0.3f + right * bodySize - up * bodySize;
    bodyCorners[6] = position + forward * bodySize * 0.3f + right * bodySize + up * bodySize;
    bodyCorners[7] = position + forward * bodySize * 0.3f - right * bodySize + up * bodySize;

    Vector4 bodyColor = { 0.8f, 0.8f, 0.8f, 1.0f };

    // ボディを描画
    DrawQuad(bodyCorners[0], bodyCorners[1], bodyCorners[2], bodyCorners[3], bodyColor, viewCamera);
    DrawQuad(bodyCorners[4], bodyCorners[5], bodyCorners[6], bodyCorners[7], bodyColor, viewCamera);
    lineRenderer_->DrawLine(bodyCorners[0], bodyCorners[4], bodyColor, viewCamera);
    lineRenderer_->DrawLine(bodyCorners[1], bodyCorners[5], bodyColor, viewCamera);
    lineRenderer_->DrawLine(bodyCorners[2], bodyCorners[6], bodyColor, viewCamera);
    lineRenderer_->DrawLine(bodyCorners[3], bodyCorners[7], bodyColor, viewCamera);

    // レンズ（円錐の先端）
    Vector3 lensCenter = position + forward * bodySize * 0.3f;
    Vector3 lensTip = position + forward * lensLength;
    float lensRadius = bodySize * 0.6f;

    // レンズの円周を描画
    constexpr int segments = 8;
    for (int i = 0; i < segments; ++i) {
        float angle1 = (float)i / segments * 6.28318f;
        float angle2 = (float)(i + 1) / segments * 6.28318f;

        Vector3 p1 = lensCenter + right * std::cos(angle1) * lensRadius + up * std::sin(angle1) * lensRadius;
        Vector3 p2 = lensCenter + right * std::cos(angle2) * lensRadius + up * std::sin(angle2) * lensRadius;

        lineRenderer_->DrawLine(p1, p2, bodyColor, viewCamera);
        lineRenderer_->DrawLine(p1, lensTip, bodyColor, viewCamera);
    }
}

void CameraGizmo::CalculateFrustumCorners(
    const Vector3& position,
    const Quaternion& rotation,
    float fov,
    float aspectRatio,
    float nearClip,
    float farClip,
    Vector3 outCorners[8]) const {

    Matrix4x4 rotMatrix = MakeRotateMatrix(rotation);

    Vector3 forward = { rotMatrix.m[2][0], rotMatrix.m[2][1], rotMatrix.m[2][2] };
    Vector3 right = { rotMatrix.m[0][0], rotMatrix.m[0][1], rotMatrix.m[0][2] };
    Vector3 up = { rotMatrix.m[1][0], rotMatrix.m[1][1], rotMatrix.m[1][2] };

    // Near/Far平面のサイズを計算
    float nearHeight = 2.0f * nearClip * std::tan(fov * 0.5f);
    float nearWidth = nearHeight * aspectRatio;
    float farHeight = 2.0f * farClip * std::tan(fov * 0.5f);
    float farWidth = farHeight * aspectRatio;

    Vector3 nearCenter = position + forward * nearClip;
    Vector3 farCenter = position + forward * farClip;

    if (outCorners) {
        // Near plane corners (時計回り、左下から)
        outCorners[0] = nearCenter - right * (nearWidth * 0.5f) - up * (nearHeight * 0.5f);
        outCorners[1] = nearCenter + right * (nearWidth * 0.5f) - up * (nearHeight * 0.5f);
        outCorners[2] = nearCenter + right * (nearWidth * 0.5f) + up * (nearHeight * 0.5f);
        outCorners[3] = nearCenter - right * (nearWidth * 0.5f) + up * (nearHeight * 0.5f);

        // Far plane corners
        outCorners[4] = farCenter - right * (farWidth * 0.5f) - up * (farHeight * 0.5f);
        outCorners[5] = farCenter + right * (farWidth * 0.5f) - up * (farHeight * 0.5f);
        outCorners[6] = farCenter + right * (farWidth * 0.5f) + up * (farHeight * 0.5f);
        outCorners[7] = farCenter - right * (farWidth * 0.5f) + up * (farHeight * 0.5f);
    }
}

void CameraGizmo::DrawQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                            const Vector4& color, Camera* viewCamera) {
    lineRenderer_->DrawLine(p0, p1, color, viewCamera);
    lineRenderer_->DrawLine(p1, p2, color, viewCamera);
    lineRenderer_->DrawLine(p2, p3, color, viewCamera);
    lineRenderer_->DrawLine(p3, p0, color, viewCamera);
}

} // namespace GameEngine

#endif // USE_IMGUI
