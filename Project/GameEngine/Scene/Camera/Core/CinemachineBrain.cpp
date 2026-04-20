#include "pch.h"
#include "CinemachineBrain.h"
#include "../Camera.h"
#include <algorithm>

namespace GameEngine {

void CinemachineBrain::Initialize(Camera* outputCamera) {
    outputCamera_ = outputCamera;
}

void CinemachineBrain::Update(float deltaTime) {
    // 最高優先度のカメラを検索
    VirtualCamera* highestPriority = FindHighestPriorityCamera();

    // アクティブカメラが変更された場合、ブレンド開始
    if (highestPriority != activeCamera_ && highestPriority != nullptr) {
        BlendToCamera(highestPriority);
    }

    // ブレンド処理
    if (isBlending_) {
        UpdateBlend(deltaTime);
    } else if (activeCamera_) {
        currentState_ = activeCamera_->GetState();
    }

    // 出力カメラに適用
    ApplyStateToOutputCamera();
}

VirtualCamera* CinemachineBrain::FindHighestPriorityCamera() const {
    VirtualCamera* highest = nullptr;
    int highestPriority = INT_MIN;

    for (VirtualCamera* vcam : virtualCameras_) {
        if (vcam && vcam->IsActive() && vcam->GetPriority() > highestPriority) {
            highestPriority = vcam->GetPriority();
            highest = vcam;
        }
    }

    return highest;
}

void CinemachineBrain::BlendToCamera(VirtualCamera* newCamera) {
    if (newCamera == nullptr) return;

    previousCamera_ = activeCamera_;
    activeCamera_ = newCamera;

    if (previousCamera_ != nullptr && defaultBlendTime_ > 0.0f) {
        blendStartState_ = currentState_;
        blendDuration_ = defaultBlendTime_;
        blendProgress_ = 0.0f;
        isBlending_ = true;
    } else {
        currentState_ = newCamera->GetState();
        isBlending_ = false;
        blendProgress_ = 1.0f;
    }
}

void CinemachineBrain::UpdateBlend(float deltaTime) {
    if (!isBlending_ || !activeCamera_) return;

    blendProgress_ += deltaTime / blendDuration_;

    if (blendProgress_ >= 1.0f) {
        blendProgress_ = 1.0f;
        isBlending_ = false;
        currentState_ = activeCamera_->GetState();
    } else {
        // イーズイン・アウト補間 (Smoothstep)
        float t = blendProgress_;
        float easedT = t * t * (3.0f - 2.0f * t);

        currentState_ = CameraState::Lerp(blendStartState_, activeCamera_->GetState(), easedT);
    }
}

void CinemachineBrain::ApplyStateToOutputCamera() {
    if (outputCamera_ == nullptr) return;

    outputCamera_->SetFovY(currentState_.fov);
    outputCamera_->SetNearClip(currentState_.nearClip);
    outputCamera_->SetFarClip(currentState_.farClip);

    if (currentState_.hasViewMatrixOverride) {
        // ビュー行列が直接指定されている場合はそのまま使用
        Matrix4x4 projectionMatrix = outputCamera_->GetProjectionMatrix();
        outputCamera_->SetViewProjectionMatrix(currentState_.viewMatrixOverride * projectionMatrix);
        outputCamera_->SetPosition(currentState_.transform.translation);
        outputCamera_->SetCameraForGpuData();
    } else {
        outputCamera_->SetTransform(currentState_.transform);
        outputCamera_->Update();
    }
}

void CinemachineBrain::RegisterVirtualCamera(VirtualCamera* vcam) {
    if (vcam == nullptr) return;

    auto it = std::find(virtualCameras_.begin(), virtualCameras_.end(), vcam);
    if (it == virtualCameras_.end()) {
        virtualCameras_.push_back(vcam);
    }
}

void CinemachineBrain::UnregisterVirtualCamera(VirtualCamera* vcam) {
    virtualCameras_.erase(
        std::remove(virtualCameras_.begin(), virtualCameras_.end(), vcam),
        virtualCameras_.end());

    if (activeCamera_ == vcam) {
        activeCamera_ = FindHighestPriorityCamera();
    }
    if (previousCamera_ == vcam) {
        previousCamera_ = nullptr;
    }
}

void CinemachineBrain::Cut(VirtualCamera* vcam) {
    if (vcam == nullptr) return;

    previousCamera_ = activeCamera_;
    activeCamera_ = vcam;
    currentState_ = vcam->GetState();
    isBlending_ = false;
    blendProgress_ = 1.0f;
}

} // namespace GameEngine
