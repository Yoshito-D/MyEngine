#include "pch.h"

#ifdef USE_IMGUI

#include "CameraEditor.h"
#include "../Camera.h"
#include "../Core/VirtualCamera.h"
#include "../Core/CinemachineBrain.h"
#include "../Core/ICinemachineComponent.h"
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

void CameraEditor::SetTargetBrain(CinemachineBrain* brain) {
    targetBrain_ = brain;
    // BrainがCameraを所有しているのでtargetCamera_を自動設定
    targetCamera_ = brain ? brain->GetOutputCamera() : nullptr;
    // brain変更時は選択状態をリセット
    targetVirtualCamera_ = nullptr;
    selectedVirtualCameraIndex_ = -1;
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
            ShowVirtualCameraTab();
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

void CameraEditor::ShowVirtualCameraTab() {
    // Brainが設定されている場合は登録一覧からVirtualCameraを選択
    if (targetBrain_) {
        const auto& vcams = targetBrain_->GetVirtualCameras();

        // 選択状態を毎フレーム検証
        if (targetVirtualCamera_) {
            int foundIndex = -1;
            for (size_t i = 0; i < vcams.size(); ++i) {
                if (vcams[i] == targetVirtualCamera_) {
                    foundIndex = static_cast<int>(i);
                    break;
                }
            }
            if (foundIndex == -1) {
                targetVirtualCamera_ = nullptr;
                selectedVirtualCameraIndex_ = -1;
            } else {
                selectedVirtualCameraIndex_ = foundIndex;
            }
        }

        // 左ペイン：カメラ一覧
        ImGui::BeginChild("VCamList", ImVec2(180, 0), true);
        ImGui::Text("Virtual Cameras (%zu)", vcams.size());
        ImGui::Separator();

        for (size_t i = 0; i < vcams.size(); ++i) {
            VirtualCamera* vcam = vcams[i];
            if (!vcam) continue;

            bool isActive = (vcam == targetBrain_->GetActiveCamera());
            ImGui::PushID(static_cast<int>(i));

            char label[128];
            const std::string& vcamName = vcam->GetName();
            if (vcamName.empty()) {
                snprintf(label, sizeof(label), "[%zu] P:%d%s", i, vcam->GetPriority(), isActive ? " *" : "");
            } else {
                snprintf(label, sizeof(label), "%s%s", vcamName.c_str(), isActive ? " *" : "");
            }

            if (ImGui::Selectable(label, selectedVirtualCameraIndex_ == static_cast<int>(i))) {
                selectedVirtualCameraIndex_ = static_cast<int>(i);
                targetVirtualCamera_ = vcam;
            }

            ImGui::PopID();
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // 右ペイン：選択中VirtualCameraの詳細
        ImGui::BeginChild("VCamInspector", ImVec2(0, 0), false);
        if (targetVirtualCamera_) {
            ShowVirtualCameraInspector(targetVirtualCamera_);
        } else {
            ImGui::TextDisabled("Select a virtual camera from the list");
        }
        ImGui::EndChild();

    } else {
        // Brainなし：単一のVirtualCameraを直接表示
        if (targetVirtualCamera_) {
            ShowVirtualCameraInspector(targetVirtualCamera_);
        } else {
            ImGui::TextDisabled("No virtual camera selected");
        }
    }
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
        // 名前編集
        char nameBuf[128];
        const std::string& currentName = vcam->GetName();
        snprintf(nameBuf, sizeof(nameBuf), "%s", currentName.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            vcam->SetName(nameBuf);
        }

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

    // コンポーネント（各コンポーネントが自身を描画）
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& components = vcam->GetComponents();
        ICinemachineComponent* toRemove = nullptr;

        if (components.empty()) {
            ImGui::TextDisabled("No components");
        } else {
            for (auto& comp : components) {
                if (!comp) continue;
                ImGui::PushID(comp.get());

                bool open = ImGui::TreeNode(comp->GetComponentName());

                // 削除ボタン（TreeNode と同じ行）
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 14.0f);
                if (ImGui::SmallButton("x")) {
                    toRemove = comp.get();
                }

                if (open) {
                    comp->DrawInspector();
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }

        if (toRemove) {
            vcam->RemoveComponent(toRemove);
        }

        ImGui::Separator();
        ShowAddComponentPopup(vcam);
    }
}

void CameraEditor::ShowAddComponentPopup(VirtualCamera* vcam) {
    if (ImGui::Button("+ Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::Text("Engine Components");
        ImGui::Separator();

        if (ImGui::MenuItem("FollowBody")) {
            if (!vcam->GetComponent<FollowBody>()) {
                vcam->AddComponent<FollowBody>();
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("OrbitalBody")) {
            if (!vcam->GetComponent<OrbitalBody>()) {
                vcam->AddComponent<OrbitalBody>();
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("LookAtAim")) {
            if (!vcam->GetComponent<LookAtAim>()) {
                vcam->AddComponent<LookAtAim>();
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("PerlinNoise")) {
            if (!vcam->GetComponent<PerlinNoise>()) {
                vcam->AddComponent<PerlinNoise>();
            }
            ImGui::CloseCurrentPopup();
        }

        if (!externalFactories_.empty()) {
            ImGui::Separator();
            ImGui::Text("App Components");
            ImGui::Separator();
            for (auto& entry : externalFactories_) {
                if (ImGui::MenuItem(entry.displayName.c_str())) {
                    entry.factory(vcam);
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
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
        ImGui::Text("FOV: %.2f rad (%.1f deg)", state.fov, state.fov * kRadToDeg);
        ImGui::Text("Near/Far: %.2f / %.2f", state.nearClip, state.farClip);

        VirtualCamera* activeVcam = brain->GetActiveCamera();
        if (activeVcam) {
            ImGui::Text("Active Camera Priority: %d", activeVcam->GetPriority());
        } else {
            ImGui::TextDisabled("No active camera");
        }
    }

    // 登録されたVirtualCamera一覧
    if (ImGui::CollapsingHeader("Registered Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& vcams = brain->GetVirtualCameras();

        // 選択状態の検証・同期
        if (targetVirtualCamera_) {
            int foundIndex = -1;
            for (size_t i = 0; i < vcams.size(); ++i) {
                if (vcams[i] == targetVirtualCamera_) {
                    foundIndex = static_cast<int>(i);
                    break;
                }
            }
            if (foundIndex == -1) {
                targetVirtualCamera_ = nullptr;
                selectedVirtualCameraIndex_ = -1;
            } else {
                selectedVirtualCameraIndex_ = foundIndex;
            }
        }

        ImGui::Text("Count: %zu", vcams.size());
        ImGui::Separator();

        for (size_t i = 0; i < vcams.size(); ++i) {
            VirtualCamera* vcam = vcams[i];
            if (!vcam) continue;

            bool isActive = (vcam == brain->GetActiveCamera());
            ImGui::PushID(static_cast<int>(i));

            char label[64];
            snprintf(label, sizeof(label), "[%zu] Priority:%d%s",
                i, vcam->GetPriority(), isActive ? " [ACTIVE]" : "");

            if (ImGui::Selectable(label, selectedVirtualCameraIndex_ == static_cast<int>(i))) {
                selectedVirtualCameraIndex_ = static_cast<int>(i);
                targetVirtualCamera_ = vcam;
            }

            ImGui::SameLine();
            bool active = vcam->IsActive();
            if (ImGui::Checkbox("##enabled", &active)) {
                vcam->SetActive(active);
            }

            ImGui::PopID();
        }
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
        float rot[3] = { euler.x * kRadToDeg, euler.y * kRadToDeg, euler.z * kRadToDeg };
        if (ImGui::DragFloat3("Rotation (deg)", rot, 1.0f)) {
            transform.SetRotationEuler({ rot[0] * kDegToRad, rot[1] * kDegToRad, rot[2] * kDegToRad });
            changed = true;
        }

        Quaternion q = transform.GetActiveQuaternion();
        ImGui::Text("Quaternion: (%.3f, %.3f, %.3f, %.3f)", q.x, q.y, q.z, q.w);
    } else {
        float rot[3] = { transform.rotation.x * kRadToDeg, transform.rotation.y * kRadToDeg, transform.rotation.z * kRadToDeg };
        if (ImGui::DragFloat3("Rotation (deg)", rot, 1.0f)) {
            transform.rotation = { rot[0] * kDegToRad, rot[1] * kDegToRad, rot[2] * kDegToRad };
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
    if (ImGui::Combo("Type##Projection", &projType, projTypes, 2)) {
        camera->SetProjectionType(static_cast<Camera::ProjectionType>(projType));
        changed = true;
    }

    // FOV
    float fov = camera->GetFovY();
    float fovDeg = fov * kRadToDeg;
    if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 1.0f, 179.0f)) {
        camera->SetFovY(fovDeg * kDegToRad);
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

    float fovDeg = state.fov * kRadToDeg;
    if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 1.0f, 179.0f)) {
        state.fov = fovDeg * kDegToRad;
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

void CameraEditor::RegisterComponentFactory(const std::string& displayName,
                                            std::function<ICinemachineComponent*(VirtualCamera*)> factory) {
    externalFactories_.push_back({ displayName, std::move(factory) });
}

} // namespace GameEngine

#endif // USE_IMGUI
