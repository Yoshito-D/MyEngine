#pragma once

#ifdef USE_IMGUI

#include "Scene/Camera/Core/CameraState.h"
#include "CameraGizmo.h"
#include <functional>
#include <string>
#include <vector>

namespace GameEngine {

class Camera;
class VirtualCamera;
class CinemachineBrain;
class LineRenderer;
class ICinemachineComponent;

/// @brief カメラエディタクラス
/// カメラのパラメータ編集、視錐台表示、VirtualCamera管理を行う
class CameraEditor {
public:
    enum class GizmoOperation {
        Translate,
        Rotate,
        Scale,
    };

    enum class GizmoMode {
        Local,
        World,
    };

    CameraEditor() = default;
    ~CameraEditor() = default;

    /// @brief 初期化
    /// @param lineRenderer ギズモ描画用のラインレンダラー
    void Initialize(LineRenderer* lineRenderer);

    /// @brief メインエディタウィンドウを表示
    void ShowEditorWindow();

    /// @brief カメラ詳細パネルを表示
    /// @param camera 編集対象のカメラ
    void ShowCameraInspector(Camera* camera);

    /// @brief VirtualCamera詳細パネルを表示
    /// @param vcam 編集対象のVirtualCamera
    void ShowVirtualCameraInspector(VirtualCamera* vcam);

    /// @brief CinemachineBrain管理パネルを表示
    /// @param brain 管理対象のBrain
    void ShowBrainInspector(CinemachineBrain* brain);

    /// @brief ギズモを描画
    /// @param viewCamera ビューに使用するカメラ
    void DrawGizmos(Camera* viewCamera);

    /// @brief viewport 上に ImGuizmo の Transform 操作ハンドルを描画
    void DrawSceneGizmos(Camera* viewCamera, float viewportX, float viewportY, float viewportWidth, float viewportHeight);

    /// @brief 編集対象のカメラを設定
    void SetTargetCamera(Camera* camera) { targetCamera_ = camera; }

    /// @brief 編集対象のVirtualCameraを設定
    void SetTargetVirtualCamera(VirtualCamera* vcam) { targetVirtualCamera_ = vcam; }

    /// @brief 編集対象のBrainを設定（出力カメラも自動的にtargetCamera_に設定される）
    void SetTargetBrain(CinemachineBrain* brain);

    /// @brief ギズモ設定を取得
    CameraGizmo::Settings& GetGizmoSettings() { return gizmo_.GetSettings(); }

    /// @brief ギズモを表示するかどうか
    void SetShowGizmo(bool show) { showGizmo_ = show; }
    bool GetShowGizmo() const { return showGizmo_; }

    /// @brief 外部コンポーネント（App側など）をエディタのAdd Componentリストに登録
    /// @param displayName ポップアップに表示される名前
    /// @param factory VirtualCamera* を受け取り ICinemachineComponent* を返すファクトリ関数
    void RegisterComponentFactory(const std::string& displayName,
                                  std::function<ICinemachineComponent*(VirtualCamera*)> factory);

private:
    /// @brief VirtualCameraタブの表示（Brain登録一覧＋インスペクタ）
    void ShowVirtualCameraTab();

    /// @brief Transform編集UI
    /// @param transform 編集対象
    /// @return 変更があった場合true
    bool EditTransform(Transform& transform);

    /// @brief 投影設定編集UI
    /// @param camera 編集対象
    /// @return 変更があった場合true
    bool EditProjectionSettings(Camera* camera);

    /// @brief CameraState編集UI
    /// @param state 編集対象
    /// @return 変更があった場合true
    bool EditCameraState(CameraState& state);

    /// @brief ギズモ設定パネル
    void ShowGizmoSettings();

    /// @brief Add Component ポップアップ
    void ShowAddComponentPopup(VirtualCamera* vcam);

    bool DrawTransformGizmo(Transform& transform, Camera* viewCamera, float viewportX, float viewportY, float viewportWidth, float viewportHeight, const char* id);
    void ShowImGuizmoControls();

    CameraGizmo gizmo_;
    Camera* targetCamera_ = nullptr;
    VirtualCamera* targetVirtualCamera_ = nullptr;
    CinemachineBrain* targetBrain_ = nullptr;

    bool showGizmo_ = true;
    bool useImGuizmo_ = true;
    bool showGizmoSettings_ = false;
    GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
    GizmoMode gizmoMode_ = GizmoMode::Local;
    int selectedVirtualCameraIndex_ = -1;

    struct ComponentFactoryEntry {
        std::string displayName;
        std::function<ICinemachineComponent*(VirtualCamera*)> factory;
    };
    std::vector<ComponentFactoryEntry> externalFactories_;

};

} // namespace GameEngine

#endif // USE_IMGUI
