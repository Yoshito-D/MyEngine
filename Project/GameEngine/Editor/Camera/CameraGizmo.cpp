#include "pch.h"

#ifdef USE_IMGUI

#include "CameraGizmo.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Core/Renderer/Pass/LineRenderer.h"
#include "Core/Window/Window.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
namespace {
constexpr float kDefaultAspectRatio = 16.0f / 9.0f;
constexpr float kMinAspectRatio = 0.0001f;
constexpr float kMinClipDistance = 0.0001f;
constexpr float kHomogeneousEpsilon = 1.0e-6f;

float NormalizeAspectRatio(float aspectRatio) {
    return aspectRatio > kMinAspectRatio ? aspectRatio : kDefaultAspectRatio;
}

float NormalizeFarClip(float nearClip, float farClip) {
    return std::max(farClip, nearClip + kMinClipDistance);
}

Vector3 NormalizeOrDefault(const Vector3& value, const Vector3& fallback) {
    return value.LengthSquared() > 1.0e-8f ? value.Normalize() : fallback;
}
} // namespace

void CameraGizmo::Initialize(LineRenderer* lineRenderer) {
    lineRenderer_ = lineRenderer;
}

void CameraGizmo::DrawFrustum(Camera* camera, Camera* viewCamera) {
    if (!camera || !viewCamera || !lineRenderer_) return;

    const float farClip = NormalizeFarClip(camera->GetNearClip(), camera->GetFarClip());
    const Matrix4x4 projectionMatrix = MakeCameraProjectionMatrix(*camera, farClip);

    Vector3 corners[8];
    CalculateFrustumCorners(camera->GetViewMatrix(), projectionMatrix, corners);

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

    DrawOrientationVectors(camera->GetViewMatrix(), viewCamera);
}

void CameraGizmo::DrawFrustum(const CameraState& state, Camera* viewCamera) {
    const float aspectRatio = viewCamera ? viewCamera->GetAspectRatio() : kDefaultAspectRatio;
    DrawFrustum(state, viewCamera, aspectRatio);
}

void CameraGizmo::DrawFrustum(const CameraState& state, Camera* viewCamera, float aspectRatio) {
    if (!lineRenderer_ || !viewCamera) return;

    const float farClip = NormalizeFarClip(state.nearClip, state.farClip);
    const Matrix4x4 viewMatrix = state.GetViewMatrix();
    const Matrix4x4 projectionMatrix = MakeCameraStateProjectionMatrix(state, aspectRatio, farClip);

    // 視錐台の8頂点を計算
    Vector3 corners[8];
    CalculateFrustumCorners(viewMatrix, projectionMatrix, corners);

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

    DrawOrientationVectors(viewMatrix, viewCamera);
}

void CameraGizmo::DrawFrustum(VirtualCamera* vcam, Camera* viewCamera) {
    if (!vcam) return;
    const float aspectRatio = viewCamera ? viewCamera->GetAspectRatio() : kDefaultAspectRatio;
    DrawFrustum(vcam->GetState(), viewCamera, aspectRatio);
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
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix,
    Vector3 outCorners[8]) const {

    if (!outCorners) return;

    const Matrix4x4 inverseViewProjection = (viewMatrix * projectionMatrix).Inverse();

    // DirectXのNDCは z=[0,1]。ここから逆変換することで、実際の投影行列と同じ視錐台を描く。
    outCorners[0] = UnprojectClipPoint({ -1.0f, -1.0f, 0.0f, 1.0f }, inverseViewProjection);
    outCorners[1] = UnprojectClipPoint({  1.0f, -1.0f, 0.0f, 1.0f }, inverseViewProjection);
    outCorners[2] = UnprojectClipPoint({  1.0f,  1.0f, 0.0f, 1.0f }, inverseViewProjection);
    outCorners[3] = UnprojectClipPoint({ -1.0f,  1.0f, 0.0f, 1.0f }, inverseViewProjection);
    outCorners[4] = UnprojectClipPoint({ -1.0f, -1.0f, 1.0f, 1.0f }, inverseViewProjection);
    outCorners[5] = UnprojectClipPoint({  1.0f, -1.0f, 1.0f, 1.0f }, inverseViewProjection);
    outCorners[6] = UnprojectClipPoint({  1.0f,  1.0f, 1.0f, 1.0f }, inverseViewProjection);
    outCorners[7] = UnprojectClipPoint({ -1.0f,  1.0f, 1.0f, 1.0f }, inverseViewProjection);
}

Matrix4x4 CameraGizmo::MakeCameraProjectionMatrix(const Camera& camera, float farClip) const {
    switch (camera.GetProjectionType()) {
    case Camera::ProjectionType::Orthographic:
        return MakeOrthographicMatrix(
            static_cast<float>(-Window::kResolutionWidth) * 0.5f,
            static_cast<float>(Window::kResolutionHeight) * 0.5f,
            static_cast<float>(Window::kResolutionWidth) * 0.5f,
            static_cast<float>(-Window::kResolutionHeight) * 0.5f,
            camera.GetNearClip(),
            farClip);
    case Camera::ProjectionType::Perspective:
    default:
        return MakePerspectiveFovMatrix(
            camera.GetFovY(),
            NormalizeAspectRatio(camera.GetAspectRatio()),
            camera.GetNearClip(),
            farClip);
    }
}

Matrix4x4 CameraGizmo::MakeCameraStateProjectionMatrix(const CameraState& state, float aspectRatio, float farClip) const {
    return MakePerspectiveFovMatrix(
        state.fov,
        NormalizeAspectRatio(aspectRatio),
        state.nearClip,
        farClip);
}

Vector3 CameraGizmo::UnprojectClipPoint(const Vector4& clipPoint, const Matrix4x4& inverseViewProjection) const {
    const Vector4 world = TransformVectorByMatrix(clipPoint, inverseViewProjection);
    if (std::abs(world.w) < kHomogeneousEpsilon) {
        return { world.x, world.y, world.z };
    }
    return { world.x / world.w, world.y / world.w, world.z / world.w };
}

void CameraGizmo::DrawOrientationVectors(const Matrix4x4& viewMatrix, Camera* viewCamera) {
    if (!lineRenderer_ || !viewCamera) return;

    const Matrix4x4 worldMatrix = viewMatrix.Inverse();
    const Vector3 position = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

    // viewMatrixOverrideを使うCameraStateでも、実際のビュー行列から向きを取り出す。
    const Vector3 forward = NormalizeOrDefault({ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    const Vector3 up = NormalizeOrDefault({ worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });

    // カメラの向きを描画
    if (settings_.showDirection) {
        lineRenderer_->DrawLine(position, position + forward * 2.0f, settings_.directionColor, viewCamera);
    }

    // 上方向ベクトルを描画
    if (settings_.showUpVector) {
        lineRenderer_->DrawLine(position, position + up * 1.0f, settings_.upVectorColor, viewCamera);
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
