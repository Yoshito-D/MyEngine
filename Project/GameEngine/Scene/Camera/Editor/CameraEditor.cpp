#include "pch.h"

#ifdef USE_IMGUI

#include "CameraEditor.h"
#include "../Camera.h"
#include "../Core/VirtualCamera.h"
#include "../Core/CinemachineBrain.h"
#include "../Components/FollowBody.h"
#include "../Components/OrbitalBody.h"
#include "../Components/LookAtAim.h"
#include "../Components/PerlinNoise.h"
#include "Core/Renderer/LineRenderer.h"
#include "externals/imgui/imgui.h"

namespace GameEngine {

void CameraEditor::Initialize(LineRenderer* lineRenderer) {
    gizmo_.Initialize(lineRenderer);
}

void CameraEditor::ShowEditorWindow() {
    if (!ImGui::Begin("Camera Editor")) {
        ImGui::End();
        return;
    }

    // ギズモ表示設定
    ImGui::Checkbox("Show Gizmo", &showGizmo_);
    ImGui::SameLine();
    if (ImGui::Button("Gizmo Settings")) {
        showGizmoSettings_ = !showGizmoSettings_;
    }

    if (showGizmoSettings_) {
        ShowGizmoSettings();
    }

    ImGui::Separator();

    // タブでカメラ種別を切り替え
    if (ImGui::BeginTabBar("CameraEditorTabs")) {
        // メインカメラタブ
        if (ImGui::BeginTabItem("Main Camera")) {
            if (targetCamera_) {
                ShowCameraInspector(targetCamera_);
            } else {
                ImGui::TextDisabled("No camera selected");
            }
            ImGui::EndTabItem();
        }

        // VirtualCameraタブ
        if (ImGui::BeginTabItem("Virtual Camera")) {
            if (targetVirtualCamera_) {
                ShowVirtualCameraInspector(targetVirtualCamera_);
            } else {
                ImGui::TextDisabled("No virtual camera selected");
            }
            ImGui::EndTabItem();
        }

        // Brainタブ
        if (ImGui::BeginTabItem("Cinemachine Brain")) {
            if (targetBrain_) {
                ShowBrainInspector(targetBrain_);
            } else {
                ImGui::TextDisabled("No brain selected");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void CameraEditor::ShowCameraInspector(Camera* camera) {
    if (!camera) return;

    ImGui::Text("Camera Properties");
    ImGui::Separator();

    // Transform
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        Transform transform = camera->GetTransform();
        if (EditTransform(transform)) {
            camera->SetTransform(transform);
        }
    }

    // Projection
    if (ImGui::CollapsingHeader("Projection", ImGuiTreeNodeFlags_DefaultOpen)) {
        EditProjectionSettings(camera);
    }

    // Info
    if (ImGui::CollapsingHeader("Info")) {
        Vector3 forward = camera->GetForward();
        ImGui::Text("Forward: (%.3f, %.3f, %.3f)", forward.x, forward.y, forward.z);

        Matrix4x4 vp = camera->GetViewProjectionMatrix();
        ImGui::Text("ViewProjection Matrix set");
    }
}

void CameraEditor::ShowVirtualCameraInspector(VirtualCamera* vcam) {
    if (!vcam) return;

    ImGui::Text("Virtual Camera Properties");
    ImGui::Separator();

    // 基本設定
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        int priority = vcam->GetPriority();
        if (ImGui::DragInt("Priority", &priority)) {
            vcam->SetPriority(priority);
        }

        bool active = vcam->IsActive();
        if (ImGui::Checkbox("Active", &active)) {
            vcam->SetActive(active);
        }
    }

    // カメラ状態
    if (ImGui::CollapsingHeader("Camera State", ImGuiTreeNodeFlags_DefaultOpen)) {
        CameraState state = vcam->GetState();
        if (EditCameraState(state)) {
            vcam->SetState(state);
        }
    }

    // ターゲット情報
    if (ImGui::CollapsingHeader("Targets")) {
        Transform* followTarget = vcam->GetFollowTarget();
        Transform* lookAtTarget = vcam->GetLookAtTarget();

        if (followTarget) {
            ImGui::Text("Follow Target: (%.2f, %.2f, %.2f)", 
                followTarget->translation.x, followTarget->translation.y, followTarget->translation.z);
        } else {
            ImGui::TextDisabled("Follow Target: None");
        }

        if (lookAtTarget) {
            ImGui::Text("LookAt Target: (%.2f, %.2f, %.2f)", 
                lookAtTarget->translation.x, lookAtTarget->translation.y, lookAtTarget->translation.z);
        } else {
            ImGui::TextDisabled("LookAt Target: None");
        }
    }

    // コンポーネント
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        // FollowBody
        if (auto* comp = vcam->GetComponent<FollowBody>()) {
            if (ImGui::TreeNode("FollowBody")) {
                EditFollowBody(comp);
                ImGui::TreePop();
            }
        }

        // OrbitalBody
        if (auto* comp = vcam->GetComponent<OrbitalBody>()) {
            if (ImGui::TreeNode("OrbitalBody")) {
                EditOrbitalBody(comp);
                ImGui::TreePop();
            }
        }

        // LookAtAim
        if (auto* comp = vcam->GetComponent<LookAtAim>()) {
            if (ImGui::TreeNode("LookAtAim")) {
                EditLookAtAim(comp);
                ImGui::TreePop();
            }
        }

        // PerlinNoise
        if (auto* comp = vcam->GetComponent<PerlinNoise>()) {
            if (ImGui::TreeNode("PerlinNoise")) {
                EditPerlinNoise(comp);
                ImGui::TreePop();
            }
        }
    }
}

void CameraEditor::ShowBrainInspector(CinemachineBrain* brain) {
    if (!brain) return;

    ImGui::Text("Cinemachine Brain");
    ImGui::Separator();

    // ブレンド設定
    if (ImGui::CollapsingHeader("Blend Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        float blendTime = brain->GetDefaultBlendTime();
        if (ImGui::DragFloat("Default Blend Time", &blendTime, 0.01f, 0.0f, 5.0f, "%.2f s")) {
            brain->SetDefaultBlendTime(blendTime);
        }
    }

    // 現在の状態
    if (ImGui::CollapsingHeader("Current State", ImGuiTreeNodeFlags_DefaultOpen)) {
        const CameraState& state = brain->GetCurrentState();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
            state.transform.translation.x, state.transform.translation.y, state.transform.translation.z);
        ImGui::Text("FOV: %.2f rad (%.1f deg)", state.fov, state.fov * 57.2958f);
        ImGui::Text("Near/Far: %.2f / %.2f", state.nearClip, state.farClip);

        VirtualCamera* activeVcam = brain->GetActiveCamera();
        if (activeVcam) {
            ImGui::Text("Active Camera Priority: %d", activeVcam->GetPriority());
        } else {
            ImGui::TextDisabled("No active camera");
        }
    }

    // 登録されたVirtualCamera一覧
    if (ImGui::CollapsingHeader("Registered Cameras")) {
        size_t count = brain->GetVirtualCameraCount();
        ImGui::Text("Count: %zu", count);

        // 注: GetVirtualCamerasの実装が必要な場合は追加
    }
}

void CameraEditor::DrawGizmos(Camera* viewCamera) {
    if (!showGizmo_ || !viewCamera) return;

    // ターゲットカメラの視錐台を描画
    if (targetCamera_ && targetCamera_ != viewCamera) {
        gizmo_.DrawFrustum(targetCamera_, viewCamera);
    }

    // VirtualCameraの視錐台を描画
    if (targetVirtualCamera_) {
        gizmo_.DrawFrustum(targetVirtualCamera_, viewCamera);
    }
}

bool CameraEditor::EditTransform(Transform& transform) {
    bool changed = false;

    float pos[3] = { transform.translation.x, transform.translation.y, transform.translation.z };
    if (ImGui::DragFloat3("Position", pos, 0.1f)) {
        transform.translation = { pos[0], pos[1], pos[2] };
        changed = true;
    }

    if (transform.IsUsingQuaternion()) {
        // クォータニオン表示（Euler角に変換して表示）
        Vector3 euler = transform.GetActiveEuler();
        float rot[3] = { euler.x * 57.2958f, euler.y * 57.2958f, euler.z * 57.2958f };
        if (ImGui::DragFloat3("Rotation (deg)", rot, 1.0f)) {
            transform.SetRotationEuler({ rot[0] / 57.2958f, rot[1] / 57.2958f, rot[2] / 57.2958f });
            changed = true;
        }

        Quaternion q = transform.GetActiveQuaternion();
        ImGui::Text("Quaternion: (%.3f, %.3f, %.3f, %.3f)", q.x, q.y, q.z, q.w);
    } else {
        float rot[3] = { transform.rotation.x * 57.2958f, transform.rotation.y * 57.2958f, transform.rotation.z * 57.2958f };
        if (ImGui::DragFloat3("Rotation (deg)", rot, 1.0f)) {
            transform.rotation = { rot[0] / 57.2958f, rot[1] / 57.2958f, rot[2] / 57.2958f };
            changed = true;
        }
    }

    float scale[3] = { transform.scale.x, transform.scale.y, transform.scale.z };
    if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.01f, 100.0f)) {
        transform.scale = { scale[0], scale[1], scale[2] };
        changed = true;
    }

    return changed;
}

bool CameraEditor::EditProjectionSettings(Camera* camera) {
    bool changed = false;

    // Projection Type
    int projType = static_cast<int>(camera->GetProjectionType());
    const char* projTypes[] = { "Perspective", "Orthographic" };
    if (ImGui::Combo("Projection", &projType, projTypes, 2)) {
        camera->SetProjectionType(static_cast<Camera::ProjectionType>(projType));
        changed = true;
    }

    // FOV
    float fov = camera->GetFovY();
    float fovDeg = fov * 57.2958f;
    if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 1.0f, 179.0f)) {
        camera->SetFovY(fovDeg / 57.2958f);
        changed = true;
    }

    // Near/Far Clip
    float nearClip = camera->GetNearClip();
    float farClip = camera->GetFarClip();

    if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, farClip - 0.001f)) {
        camera->SetNearClip(nearClip);
        changed = true;
    }

    if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, nearClip + 0.001f, 100000.0f)) {
        camera->SetFarClip(farClip);
        changed = true;
    }

    return changed;
}

bool CameraEditor::EditCameraState(CameraState& state) {
    bool changed = false;

    changed |= EditTransform(state.transform);

    float fovDeg = state.fov * 57.2958f;
    if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 1.0f, 179.0f)) {
        state.fov = fovDeg / 57.2958f;
        changed = true;
    }

    if (ImGui::DragFloat("Near Clip", &state.nearClip, 0.01f, 0.001f, state.farClip - 0.001f)) {
        changed = true;
    }

    if (ImGui::DragFloat("Far Clip", &state.farClip, 1.0f, state.nearClip + 0.001f, 100000.0f)) {
        changed = true;
    }

    return changed;
}

void CameraEditor::EditFollowBody(ICinemachineComponent* component) {
    auto* follow = static_cast<FollowBody*>(component);
    if (!follow) return;

    bool enabled = follow->IsEnabled();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        follow->SetEnabled(enabled);
    }

    Vector3 offset = follow->GetOffset();
    float off[3] = { offset.x, offset.y, offset.z };
    if (ImGui::DragFloat3("Offset", off, 0.1f)) {
        follow->SetOffset({ off[0], off[1], off[2] });
    }

    Vector3 damping = follow->GetDamping();
    float damp[3] = { damping.x, damping.y, damping.z };
    if (ImGui::DragFloat3("Damping", damp, 0.1f, 0.0f, 50.0f)) {
        follow->SetDamping({ damp[0], damp[1], damp[2] });
    }
}

void CameraEditor::EditOrbitalBody(ICinemachineComponent* component) {
    auto* orbital = static_cast<OrbitalBody*>(component);
    if (!orbital) return;

    bool enabled = orbital->IsEnabled();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        orbital->SetEnabled(enabled);
    }

    float distance = orbital->GetDistance();
    if (ImGui::DragFloat("Distance", &distance, 0.1f, 0.5f, 1000.0f)) {
        orbital->SetDistance(distance);
    }

    float yaw = orbital->GetYaw() * 57.2958f;
    if (ImGui::SliderFloat("Yaw (deg)", &yaw, -180.0f, 180.0f)) {
        orbital->SetYaw(yaw / 57.2958f);
    }

    float pitch = orbital->GetPitch() * 57.2958f;
    if (ImGui::SliderFloat("Pitch (deg)", &pitch, -89.0f, 89.0f)) {
        orbital->SetPitch(pitch / 57.2958f);
    }

    Vector3 pivot = orbital->GetPivotTarget();
    float piv[3] = { pivot.x, pivot.y, pivot.z };
    if (ImGui::DragFloat3("Pivot Target", piv, 0.1f)) {
        orbital->SetPivotTarget({ piv[0], piv[1], piv[2] });
    }
}

void CameraEditor::EditLookAtAim(ICinemachineComponent* component) {
    auto* lookAt = static_cast<LookAtAim*>(component);
    if (!lookAt) return;

    bool enabled = lookAt->IsEnabled();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        lookAt->SetEnabled(enabled);
    }

    float damping = lookAt->GetDamping();
    if (ImGui::DragFloat("Damping", &damping, 0.1f, 0.0f, 50.0f)) {
        lookAt->SetDamping(damping);
    }

    Vector3 offset = lookAt->GetOffset();
    float off[3] = { offset.x, offset.y, offset.z };
    if (ImGui::DragFloat3("Offset", off, 0.1f)) {
        lookAt->SetOffset({ off[0], off[1], off[2] });
    }
}

void CameraEditor::EditPerlinNoise(ICinemachineComponent* component) {
    auto* noise = static_cast<PerlinNoise*>(component);
    if (!noise) return;

    bool enabled = noise->IsEnabled();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        noise->SetEnabled(enabled);
    }

    ImGui::Text("Shaking: %s", noise->IsShaking() ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Test Shake");

    static float testAmplitude = 0.3f;
    static float testFrequency = 10.0f;
    static float testDuration = 0.5f;

    ImGui::DragFloat("Amplitude", &testAmplitude, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Frequency", &testFrequency, 0.1f, 0.1f, 50.0f);
    ImGui::DragFloat("Duration", &testDuration, 0.01f, 0.1f, 5.0f);

    if (ImGui::Button("Trigger Shake")) {
        noise->Shake(testAmplitude, testFrequency, testDuration);
    }
}

void CameraEditor::ShowGizmoSettings() {
    ImGui::BeginChild("GizmoSettings", ImVec2(0, 150), true);
    ImGui::Text("Gizmo Settings");
    ImGui::Separator();

    auto& settings = gizmo_.GetSettings();

    ImGui::Checkbox("Show Frustum", &settings.showFrustum);
    ImGui::Checkbox("Show Direction", &settings.showDirection);
    ImGui::Checkbox("Show Up Vector", &settings.showUpVector);
    ImGui::Checkbox("Show Near Plane", &settings.showNearPlane);
    ImGui::Checkbox("Show Far Plane", &settings.showFarPlane);

    ImGui::DragFloat("Frustum Scale", &settings.frustumScale, 0.01f, 0.01f, 1.0f);

    if (ImGui::TreeNode("Colors")) {
        ImGui::ColorEdit4("Frustum", &settings.frustumColor.x);
        ImGui::ColorEdit4("Near Plane", &settings.nearPlaneColor.x);
        ImGui::ColorEdit4("Far Plane", &settings.farPlaneColor.x);
        ImGui::ColorEdit4("Direction", &settings.directionColor.x);
        ImGui::ColorEdit4("Up Vector", &settings.upVectorColor.x);
        ImGui::TreePop();
    }

    ImGui::EndChild();
}

} // namespace GameEngine

#endif // USE_IMGUI
