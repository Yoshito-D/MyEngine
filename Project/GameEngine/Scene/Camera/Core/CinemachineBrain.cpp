#include "pch.h"
#include "CinemachineBrain.h"
#include "../Camera.h"
#include <algorithm>

namespace GameEngine {

void CinemachineBrain::Initialize(std::unique_ptr<Camera> outputCamera) {
    outputCamera_ = std::move(outputCamera);
}

void CinemachineBrain::Update(float deltaTime) {
    // 登録済みVirtualCameraを全てUpdate
    for (VirtualCamera* vcam : virtualCameras_) {
        if (vcam && vcam->IsActive()) {
            vcam->Update(deltaTime);
        }
    }

    // 最高優先度のカメラを検索
    VirtualCamera* highestPriority = FindHighestPriorityCamera();

    // アクティブカメラが変更された場合、ブレンド開始
    if (highestPriority != activeCamera_ && highestPriority != nullptr) {
        BlendToCamera(highestPriority);
    }

    // ブレンド処理
    if (!blendStack_.empty()) {
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

    if (activeCamera_ != nullptr && defaultBlendTime_ > 0.0f) {
        // 現在のcurrentState_を開始状態としてスタックに積む（連続ブレンド対応）
        blendStack_.push({ currentState_, newCamera, defaultBlendTime_, 0.0f });
    } else {
        // 前のカメラがないかブレンド時間が0の場合は即座に切り替え
        currentState_ = newCamera->GetState();
        while (!blendStack_.empty()) { blendStack_.pop(); }
    }

    activeCamera_ = newCamera;
}

void CinemachineBrain::UpdateBlend(float deltaTime) {
    if (blendStack_.empty() || !activeCamera_) return;

    BlendLayer& layer = blendStack_.top();
    layer.progress += deltaTime / layer.duration;

    if (layer.progress >= 1.0f) {
        // このレイヤーのブレンド完了
        currentState_ = layer.toCamera->GetState();
        blendStack_.pop();
    } else {
        // イーズイン・アウト補間 (Smoothstep)
        float t = layer.progress;
        float easedT = t * t * (3.0f - 2.0f * t);
        currentState_ = CameraState::Lerp(layer.fromState, layer.toCamera->GetState(), easedT);
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
}

void CinemachineBrain::Cut(VirtualCamera* vcam) {
    if (vcam == nullptr) return;

    activeCamera_ = vcam;
    currentState_ = vcam->GetState();
    while (!blendStack_.empty()) { blendStack_.pop(); }
}

} // namespace GameEngine
