#include "pch.h"

#ifdef USE_IMGUI

#include "CameraEditor.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Components/FollowBody.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "Scene/Camera/Components/LookAtAim.h"
#include "Scene/Camera/Components/PerlinNoise.h"
#include "Core/Renderer/Pass/LineRenderer.h"
#include "Utility/ImGuiHelper.h"
#include "Utility/MathUtils.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <string>

namespace GameEngine {
namespace {

const char* Tr(const char* japanese, const char* english) {
    return ImGuiHelper::Localize({ japanese, english });
}

std::string StableWindowLabel(const char* visibleLabel, const char* stableId) {
    return std::string(visibleLabel) + "###" + stableId;
}

ImGuizmo::OPERATION ToImGuizmoOperation(CameraEditor::GizmoOperation operation) {
    switch (operation) {
    case CameraEditor::GizmoOperation::Rotate:
        return ImGuizmo::ROTATE;
    case CameraEditor::GizmoOperation::Scale:
        return ImGuizmo::SCALE;
    case CameraEditor::GizmoOperation::Translate:
    default:
        return ImGuizmo::TRANSLATE;
    }
}

ImGuizmo::MODE ToImGuizmoMode(CameraEditor::GizmoMode mode) {
    return mode == CameraEditor::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

Transform MatrixToTransform(const Matrix4x4& matrix) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    float translation[3]{};
    float rotationDegrees[3]{};
    float scale[3]{};
    ImGuizmo::DecomposeMatrixToComponents(&matrix.m[0][0], translation, rotationDegrees, scale);

    Transform transform{};
    transform.translation = Vector3(translation[0], translation[1], translation[2]);
    transform.scale = Vector3(scale[0], scale[1], scale[2]);
    const Vector3 eulerRadians(
        rotationDegrees[0] * kDegToRad,
        rotationDegrees[1] * kDegToRad,
        rotationDegrees[2] * kDegToRad);
    transform.SetRotationQuaternion(eulerRadians.ToQuaternion().Normalize());
    return transform;
}

} // namespace

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
    const std::string windowLabel = StableWindowLabel(Tr("カメラエディタ", "Camera Editor"), "CameraEditor");
    if (!ImGui::Begin(windowLabel.c_str())) {
        ImGui::End();
        return;
    }

    // ギズモ表示設定
    ImGui::Checkbox(Tr("ギズモ表示", "Show Gizmo"), &showGizmo_);
    ImGui::SameLine();
    ImGui::Checkbox(Tr("ImGuizmoを使用", "Use ImGuizmo"), &useImGuizmo_);
    ImGui::SameLine();
    if (ImGui::Button(Tr("ギズモ設定", "Gizmo Settings"))) {
        showGizmoSettings_ = !showGizmoSettings_;
    }

    if (showGizmoSettings_) {
        ShowGizmoSettings();
    }

    if (useImGuizmo_) {
        ShowImGuizmoControls();
    }

    ImGui::Separator();

    // タブでカメラ種別を切り替え
    if (ImGui::BeginTabBar("CameraEditorTabs")) {
        // メインカメラタブ
        const std::string mainCameraTab = std::string(Tr("メインカメラ", "Main Camera")) + "###MainCameraTab";
        if (ImGui::BeginTabItem(mainCameraTab.c_str())) {
            if (targetCamera_) {
                ShowCameraInspector(targetCamera_);
            } else {
                ImGui::TextDisabled("%s", Tr("カメラが選択されていません", "No camera selected"));
            }
            ImGui::EndTabItem();
        }

        // VirtualCameraタブ
        const std::string virtualCameraTab = std::string(Tr("仮想カメラ", "Virtual Camera")) + "###VirtualCameraTab";
        if (ImGui::BeginTabItem(virtualCameraTab.c_str())) {
            ShowVirtualCameraTab();
            ImGui::EndTabItem();
        }

        // Brainタブ
        const std::string brainTab = std::string(Tr("Cinemachineブレイン", "Cinemachine Brain")) + "###CinemachineBrainTab";
        if (ImGui::BeginTabItem(brainTab.c_str())) {
            if (targetBrain_) {
                ShowBrainInspector(targetBrain_);
            } else {
                ImGui::TextDisabled("%s", Tr("ブレインが選択されていません", "No brain selected"));
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
        ImGui::Text("%s (%zu)", Tr("仮想カメラ", "Virtual Cameras"), vcams.size());
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
            ImGui::TextDisabled("%s", Tr("リストから仮想カメラを選択してください", "Select a virtual camera from the list"));
        }
        ImGui::EndChild();

    } else {
        // Brainなし：単一のVirtualCameraを直接表示
        if (targetVirtualCamera_) {
            ShowVirtualCameraInspector(targetVirtualCamera_);
        } else {
            ImGui::TextDisabled("%s", Tr("仮想カメラが選択されていません", "No virtual camera selected"));
        }
    }
}

void CameraEditor::ShowCameraInspector(Camera* camera) {
    if (!camera) return;

    ImGui::Text("%s", Tr("カメラプロパティ", "Camera Properties"));
    ImGui::Separator();

    // Transform
    const std::string transformHeader = std::string(Tr("トランスフォーム", "Transform")) + "###CameraTransform";
    if (ImGui::CollapsingHeader(transformHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        Transform transform = camera->GetTransform();
        if (EditTransform(transform)) {
            camera->SetTransform(transform);
        }
    }

    // Projection
    const std::string projectionHeader = std::string(Tr("投影", "Projection")) + "###CameraProjection";
    if (ImGui::CollapsingHeader(projectionHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        EditProjectionSettings(camera);
    }

    // Info
    const std::string infoHeader = std::string(Tr("情報", "Info")) + "###CameraInfo";
    if (ImGui::CollapsingHeader(infoHeader.c_str())) {
        Vector3 forward = camera->GetForward();
        ImGui::Text("%s: (%.3f, %.3f, %.3f)", Tr("前方", "Forward"), forward.x, forward.y, forward.z);

        Matrix4x4 vp = camera->GetViewProjectionMatrix();
        (void)vp;
        ImGui::Text("%s", Tr("ViewProjection 行列設定済み", "ViewProjection Matrix set"));
    }
}

void CameraEditor::ShowVirtualCameraInspector(VirtualCamera* vcam) {
    if (!vcam) return;

    ImGui::Text("%s", Tr("仮想カメラプロパティ", "Virtual Camera Properties"));
    ImGui::Separator();

    // 基本設定
    const std::string settingsHeader = std::string(Tr("設定", "Settings")) + "###VirtualCameraSettings";
    if (ImGui::CollapsingHeader(settingsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        // 名前編集
        char nameBuf[128];
        const std::string& currentName = vcam->GetName();
        snprintf(nameBuf, sizeof(nameBuf), "%s", currentName.c_str());
        if (ImGui::InputText(Tr("名前", "Name"), nameBuf, sizeof(nameBuf))) {
            vcam->SetName(nameBuf);
        }

        int priority = vcam->GetPriority();
        if (ImGui::DragInt(Tr("優先度", "Priority"), &priority)) {
            vcam->SetPriority(priority);
        }

        bool active = vcam->IsActive();
        if (ImGui::Checkbox(Tr("有効", "Active"), &active)) {
            vcam->SetActive(active);
        }
    }

    // カメラ状態
    const std::string stateHeader = std::string(Tr("カメラ状態", "Camera State")) + "###VirtualCameraState";
    if (ImGui::CollapsingHeader(stateHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        CameraState state = vcam->GetState();
        if (EditCameraState(state)) {
            vcam->SetState(state);
        }
    }

    // ターゲット情報
    const std::string targetsHeader = std::string(Tr("ターゲット", "Targets")) + "###VirtualCameraTargets";
    if (ImGui::CollapsingHeader(targetsHeader.c_str())) {
        Transform* followTarget = vcam->GetFollowTarget();
        Transform* lookAtTarget = vcam->GetLookAtTarget();

        if (followTarget) {
            ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("追従ターゲット", "Follow Target"),
                followTarget->translation.x, followTarget->translation.y, followTarget->translation.z);
        } else {
            ImGui::TextDisabled("%s", Tr("追従ターゲット: なし", "Follow Target: None"));
        }

        if (lookAtTarget) {
            ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("注視ターゲット", "LookAt Target"),
                lookAtTarget->translation.x, lookAtTarget->translation.y, lookAtTarget->translation.z);
        } else {
            ImGui::TextDisabled("%s", Tr("注視ターゲット: なし", "LookAt Target: None"));
        }
    }

    // コンポーネント（各コンポーネントが自身を描画）
    const std::string componentsHeader = std::string(Tr("コンポーネント", "Components")) + "###VirtualCameraComponents";
    if (ImGui::CollapsingHeader(componentsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& components = vcam->GetComponents();
        ICinemachineComponent* toRemove = nullptr;

        if (components.empty()) {
            ImGui::TextDisabled("%s", Tr("コンポーネントなし", "No components"));
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
    if (ImGui::Button(Tr("+ コンポーネント追加", "+ Add Component"))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::Text("%s", Tr("エンジンコンポーネント", "Engine Components"));
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
            ImGui::Text("%s", Tr("アプリコンポーネント", "App Components"));
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

    ImGui::Text("%s", Tr("Cinemachine ブレイン", "Cinemachine Brain"));
    ImGui::Separator();

    // ブレンド設定
    const std::string blendHeader = std::string(Tr("ブレンド設定", "Blend Settings")) + "###BrainBlendSettings";
    if (ImGui::CollapsingHeader(blendHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float blendTime = brain->GetDefaultBlendTime();
        if (ImGui::DragFloat(Tr("デフォルトブレンド時間", "Default Blend Time"), &blendTime, 0.01f, 0.0f, 5.0f, "%.2f s")) {
            brain->SetDefaultBlendTime(blendTime);
        }
    }

    // 現在の状態
    const std::string currentStateHeader = std::string(Tr("現在の状態", "Current State")) + "###BrainCurrentState";
    if (ImGui::CollapsingHeader(currentStateHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        const CameraState& state = brain->GetCurrentState();
        ImGui::Text("%s: (%.2f, %.2f, %.2f)",
            Tr("位置", "Position"),
            state.transform.translation.x, state.transform.translation.y, state.transform.translation.z);
        ImGui::Text("%s: %.1f deg", Tr("視野角", "FOV"), state.fov * kRadToDeg);
        ImGui::Text("%s: %.2f / %.2f", Tr("Near/Far", "Near/Far"), state.nearClip, state.farClip);

        VirtualCamera* activeVcam = brain->GetActiveCamera();
        if (activeVcam) {
            ImGui::Text("%s: %d", Tr("アクティブカメラ優先度", "Active Camera Priority"), activeVcam->GetPriority());
        } else {
            ImGui::TextDisabled("%s", Tr("アクティブカメラなし", "No active camera"));
        }
    }

    // 登録されたVirtualCamera一覧
    const std::string registeredHeader = std::string(Tr("登録済みカメラ", "Registered Cameras")) + "###BrainRegisteredCameras";
    if (ImGui::CollapsingHeader(registeredHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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

        ImGui::Text("%s: %zu", Tr("数", "Count"), vcams.size());
        ImGui::Separator();

        for (size_t i = 0; i < vcams.size(); ++i) {
            VirtualCamera* vcam = vcams[i];
            if (!vcam) continue;

            bool isActive = (vcam == brain->GetActiveCamera());
            ImGui::PushID(static_cast<int>(i));

            char label[64];
            snprintf(label, sizeof(label), "[%zu] %s:%d%s",
                i,
                Tr("優先度", "Priority"),
                vcam->GetPriority(),
                isActive ? Tr(" [有効]", " [ACTIVE]") : "");

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

    // ImGuizmo と同じ対象だけを表示し、複数カメラの視錐台で編集対象を見失わないようにする。
    if (targetVirtualCamera_) {
        gizmo_.DrawFrustum(targetVirtualCamera_, viewCamera);
        return;
    }

    // VirtualCameraが未選択の場合のみ、直接指定されたCameraの視錐台を描画
    if (targetCamera_ && targetCamera_ != viewCamera) {
        gizmo_.DrawFrustum(targetCamera_, viewCamera);
    }
}

void CameraEditor::DrawSceneGizmos(Camera* viewCamera, float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
    if (!showGizmo_ || !useImGuizmo_ || !viewCamera || viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
        return;
    }

    if (targetVirtualCamera_) {
        CameraState state = targetVirtualCamera_->GetState();
        if (DrawTransformGizmo(state.transform, viewCamera, viewportX, viewportY, viewportWidth, viewportHeight, "VirtualCamera")) {
            state.hasViewMatrixOverride = false;
            targetVirtualCamera_->SetState(state);
        }
        return;
    }

    if (targetCamera_ && targetCamera_ != viewCamera) {
        Transform transform = targetCamera_->GetTransform();
        if (DrawTransformGizmo(transform, viewCamera, viewportX, viewportY, viewportWidth, viewportHeight, "MainCamera")) {
            targetCamera_->SetTransform(transform);
            targetCamera_->Update();
        }
    }
}

bool CameraEditor::EditTransform(Transform& transform) {
    bool changed = false;

    changed |= ImGuiHelper::DrawVec3Control(Tr("位置", "Position"), transform.translation, 0.0f, 120.0f, 0.1f);

    Vector3 euler = transform.GetActiveEuler();
    if (ImGuiHelper::DrawEulerDegreesControl(Tr("回転 (deg)", "Rotation (deg)"), euler, 0.0f, 120.0f, 0.1f)) {
        transform.SetRotationQuaternion(euler.ToQuaternion().Normalize());
        changed = true;
    }

    changed |= ImGuiHelper::DrawVec3Control(Tr("スケール", "Scale"), transform.scale, 1.0f, 120.0f, 0.01f, 0.01f, 100.0f);

    return changed;
}

bool CameraEditor::EditProjectionSettings(Camera* camera) {
    bool changed = false;

    // Projection Type
    int projType = static_cast<int>(camera->GetProjectionType());
    const char* projTypes[] = { Tr("透視投影", "Perspective"), Tr("平行投影", "Orthographic") };
    const std::string projectionTypeLabel = std::string(Tr("投影タイプ", "Type")) + "##Projection";
    if (ImGui::Combo(projectionTypeLabel.c_str(), &projType, projTypes, 2)) {
        camera->SetProjectionType(static_cast<Camera::ProjectionType>(projType));
        changed = true;
    }

    // FOV
    float fov = camera->GetFovY();
    float fovDeg = fov * kRadToDeg;
    const std::string fovLabel = std::string(Tr("視野角 (deg)", "FOV (deg)")) + "##CameraProjectionFov";
    if (ImGui::SliderFloat(fovLabel.c_str(), &fovDeg, 1.0f, 179.0f)) {
        camera->SetFovY(fovDeg * kDegToRad);
        changed = true;
    }

    // Near/Far Clip
    float nearClip = camera->GetNearClip();
    float farClip = camera->GetFarClip();

    const std::string nearClipLabel = std::string(Tr("Near クリップ", "Near Clip")) + "##CameraProjectionNear";
    if (ImGui::DragFloat(nearClipLabel.c_str(), &nearClip, 0.01f, 0.001f, farClip - 0.001f)) {
        camera->SetNearClip(nearClip);
        changed = true;
    }

    const std::string farClipLabel = std::string(Tr("Far クリップ", "Far Clip")) + "##CameraProjectionFar";
    if (ImGui::DragFloat(farClipLabel.c_str(), &farClip, 1.0f, nearClip + 0.001f, 100000.0f)) {
        camera->SetFarClip(farClip);
        changed = true;
    }

    return changed;
}

bool CameraEditor::EditCameraState(CameraState& state) {
    bool changed = false;

    changed |= EditTransform(state.transform);

    float fovDeg = state.fov * kRadToDeg;
    const std::string fovLabel = std::string(Tr("視野角 (deg)", "FOV (deg)")) + "##CameraStateFov";
    if (ImGui::SliderFloat(fovLabel.c_str(), &fovDeg, 1.0f, 179.0f)) {
        state.fov = fovDeg * kDegToRad;
        changed = true;
    }

    const std::string nearClipLabel = std::string(Tr("Near クリップ", "Near Clip")) + "##CameraStateNear";
    if (ImGui::DragFloat(nearClipLabel.c_str(), &state.nearClip, 0.01f, 0.001f, state.farClip - 0.001f)) {
        changed = true;
    }

    const std::string farClipLabel = std::string(Tr("Far クリップ", "Far Clip")) + "##CameraStateFar";
    if (ImGui::DragFloat(farClipLabel.c_str(), &state.farClip, 1.0f, state.nearClip + 0.001f, 100000.0f)) {
        changed = true;
    }

    return changed;
}

void CameraEditor::ShowGizmoSettings() {
    ImGui::BeginChild("GizmoSettings", ImVec2(0, 150), true);
    ImGui::Text("%s", Tr("ギズモ設定", "Gizmo Settings"));
    ImGui::Separator();

    auto& settings = gizmo_.GetSettings();

    ImGui::Checkbox(Tr("視錐台を表示", "Show Frustum"), &settings.showFrustum);
    ImGui::Checkbox(Tr("方向を表示", "Show Direction"), &settings.showDirection);
    ImGui::Checkbox(Tr("上方向を表示", "Show Up Vector"), &settings.showUpVector);
    ImGui::Checkbox(Tr("Near 面を表示", "Show Near Plane"), &settings.showNearPlane);
    ImGui::Checkbox(Tr("Far 面を表示", "Show Far Plane"), &settings.showFarPlane);

    const std::string colorsLabel = std::string(Tr("色", "Colors")) + "###CameraGizmoColors";
    if (ImGui::TreeNode(colorsLabel.c_str())) {
        ImGui::ColorEdit4(Tr("視錐台", "Frustum"), &settings.frustumColor.x);
        ImGui::ColorEdit4(Tr("Near 面", "Near Plane"), &settings.nearPlaneColor.x);
        ImGui::ColorEdit4(Tr("Far 面", "Far Plane"), &settings.farPlaneColor.x);
        ImGui::ColorEdit4(Tr("方向", "Direction"), &settings.directionColor.x);
        ImGui::ColorEdit4(Tr("上方向", "Up Vector"), &settings.upVectorColor.x);
        ImGui::TreePop();
    }

    ImGui::EndChild();
}

bool CameraEditor::DrawTransformGizmo(Transform& transform, Camera* viewCamera, float viewportX, float viewportY, float viewportWidth, float viewportHeight, const char* id) {
    Matrix4x4 worldMatrix = MakeAffineMatrix(transform);
    Matrix4x4 viewMatrix = viewCamera->GetViewMatrix();
    Matrix4x4 projectionMatrix = viewCamera->GetProjectionMatrix();

    ImGui::PushID(id ? id : "CameraTransformGizmo");
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
    ImGuizmo::SetOrthographic(viewCamera->GetProjectionType() == Camera::ProjectionType::Orthographic);
    ImGuizmo::Manipulate(
        &viewMatrix.m[0][0],
        &projectionMatrix.m[0][0],
        ToImGuizmoOperation(gizmoOperation_),
        ToImGuizmoMode(gizmoMode_),
        &worldMatrix.m[0][0]);

    if (!ImGuizmo::IsUsing()) {
        ImGui::PopID();
        return false;
    }

    transform = MatrixToTransform(worldMatrix);
    ImGui::PopID();
    return true;
}

void CameraEditor::ShowImGuizmoControls() {
    ImGui::BeginChild("CameraImGuizmoControls", ImVec2(0, 74), true);
    ImGui::Text("ImGuizmo");

    int operation = static_cast<int>(gizmoOperation_);
    const char* operations[] = { Tr("移動", "Translate"), Tr("回転", "Rotate"), Tr("スケール", "Scale") };
    const std::string operationLabel = std::string(Tr("操作", "Operation")) + "##CameraImGuizmo";
    if (ImGui::Combo(operationLabel.c_str(), &operation, operations, IM_ARRAYSIZE(operations))) {
        gizmoOperation_ = static_cast<GizmoOperation>(operation);
    }

    int mode = static_cast<int>(gizmoMode_);
    const char* modes[] = { Tr("ローカル", "Local"), Tr("ワールド", "World") };
    const std::string modeLabel = std::string(Tr("空間", "Mode")) + "##CameraImGuizmo";
    if (ImGui::Combo(modeLabel.c_str(), &mode, modes, IM_ARRAYSIZE(modes))) {
        gizmoMode_ = static_cast<GizmoMode>(mode);
    }

    ImGui::EndChild();
}

void CameraEditor::RegisterComponentFactory(const std::string& displayName,
                                            std::function<ICinemachineComponent*(VirtualCamera*)> factory) {
    externalFactories_.push_back({ displayName, std::move(factory) });
}

} // namespace GameEngine

#endif // USE_IMGUI
