#include "pch.h"
#include "CinemachineBrain.h"
#include "../Camera.h"
#include <algorithm>

namespace GameEngine {

void CinemachineBrain::Initialize(std::unique_ptr<Camera> outputCamera) {
    outputCamera_ = std::move(outputCamera);
}

void CinemachineBrain::Update(float deltaTime) {
    // 選択中だけでなく全候補を先に更新し、優先度が切り替わった瞬間にも
    // 新しいカメラの追従状態がそのフレームのターゲット位置へ追いついているようにする。
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

void CinemachineBrain::UpdateEditorPreview(float deltaTime, VirtualCamera* editorDrivenCamera) {
    if (editorDrivenCamera && editorDrivenCamera->IsActive()) {
        // Edit/Paused中はゲーム用VirtualCameraの追従処理を止め、
        // シーン閲覧に必要なDebugCameraだけ入力を反映する。
        editorDrivenCamera->Update(deltaTime);
    }

    // 登録済みカメラのstateは更新せず、現在の優先度だけで出力先を選ぶ。
    VirtualCamera* highestPriority = FindHighestPriorityCamera();

    if (highestPriority != activeCamera_ && highestPriority != nullptr) {
        // 停止中のプレビューでは時間経過ブレンドを進めず、編集対象の姿勢を即座に確認できるようにする。
        Cut(highestPriority);
    } else if (activeCamera_) {
        // ブレンド中にPausedへ入った場合は現在の見た目を保持し、通常時だけ編集後のstateを出力へ反映する。
        if (blendStack_.empty()) {
            currentState_ = activeCamera_->GetState();
        }
    }

    ApplyStateToOutputCamera();
}

VirtualCamera* CinemachineBrain::FindHighestPriorityCamera() const {
    VirtualCamera* highest = nullptr;
    int highestPriority = INT_MIN;

    // 同一優先度では先に登録されたカメラを維持し、毎フレーム選択が揺れないよう厳密な > で比較する。
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
        // 途中で再切り替えされても元VirtualCameraの古いstateへ戻らず、現在表示中の姿勢からつなぐ。
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

    // Stack最上段の切り替え要求だけをこのフレームで進め、各Layerが保持する開始stateから補間する。
    BlendLayer& layer = blendStack_.top();
    layer.progress += deltaTime / layer.duration;

    if (layer.progress >= 1.0f) {
        // このレイヤーのブレンド完了
        currentState_ = layer.toCamera->GetState();
        blendStack_.pop();
    } else {
        // イーズイン・アウト補間 (Smoothstep)
        // Smoothstepで始端・終端の速度を0にし、カメラ切り替え時の速度段差を抑える。
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
        // 通常のCamera::UpdateはTransformからViewを再計算するため、Override時はProjectionとの合成と
        // GPU定数更新までをここで明示的に完了する。
        Matrix4x4 projectionMatrix = outputCamera_->GetProjectionMatrix();
        outputCamera_->SetViewMatrix(currentState_.viewMatrixOverride);
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

    // 選択中の候補が消えた場合は残りから即座に選び直し、破棄済みポインターを保持しない。
    if (activeCamera_ == vcam) {
        activeCamera_ = FindHighestPriorityCamera();
    }
}

void CinemachineBrain::Cut(VirtualCamera* vcam) {
    if (vcam == nullptr) return;

    // Cut後に古いBlendが再適用されないよう、出力状態と選択先を同時に更新してStackを空にする。
    activeCamera_ = vcam;
    currentState_ = vcam->GetState();
    while (!blendStack_.empty()) { blendStack_.pop(); }
}

} // namespace GameEngine
