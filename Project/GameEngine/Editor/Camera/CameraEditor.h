#pragma once

#include <cstdint>
#ifdef USE_IMGUI

#include "GameEngine/Scene/Camera/Core/CameraState.h"
#include "GameEngine/Editor/Camera/CameraGizmo.h"
#include <functional>
#include <string>
#include <vector>

namespace GameEngine {

class SceneWorld;
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

   /// @brief カメラの生成・削除で使用するWorldを関連付ける。
   void SetWorld(SceneWorld* world) { world_ = world; }

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
   void DrawSceneGizmos(
      Camera* viewCamera, float viewportX, float viewportY, float viewportWidth, float viewportHeight);

   /// @brief 編集対象のカメラを設定
   void SetTargetCamera(Camera* camera) { targetCamera_ = camera; }

   /// @brief 編集対象のVirtualCameraを設定
   void SetTargetVirtualCamera(VirtualCamera* vcam) { targetVirtualCamera_ = vcam; }

   /// @brief 編集対象のBrainを設定（出力カメラも自動的にtargetCamera_に設定される）
   void SetTargetBrain(CinemachineBrain* brain);

   /// @brief ギズモ設定を取得
   CameraGizmo::Settings GetGizmoSettings() const { return gizmo_.DescribeSettings(); }

   /// @brief カメラギズモの描画設定を検証して適用する。
   void ConfigureGizmos(const CameraGizmo::Settings& settings) { gizmo_.Configure(settings); }

   /// @brief ギズモを表示するかどうか
   void SetShowGizmo(bool show) { showGizmo_ = show; }

   bool GetShowGizmo() const { return showGizmo_; }

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

   bool DrawTransformGizmo(Transform& transform, Camera* viewCamera, float viewportX, float viewportY,
      float viewportWidth, float viewportHeight, const char* id);
   void ShowImGuizmoControls();

   SceneWorld* world_ = nullptr;                  ///< 所属シーンのオブジェクトと実行時状態を参照するワールド。
   CameraGizmo gizmo_;                            ///< カメラの位置・向き・視錐台を表示するギズモ。
   Camera* targetCamera_ = nullptr;               ///< 直接編集する出力カメラへの参照。
   VirtualCamera* targetVirtualCamera_ = nullptr; ///< 編集対象の仮想カメラへの参照。
   CinemachineBrain* targetBrain_ = nullptr;      ///< 仮想カメラの選択と合成を管理するBrainへの参照。

   bool showGizmo_ = true;                                     ///< カメラのギズモを表示するかどうか。
   bool useImGuizmo_ = true;                                   ///< ImGuizmoによる移動・回転・拡縮操作を使うかどうか。
   bool showGizmoSettings_ = false;                            ///< ギズモの表示設定を開いているかどうか。
   GizmoOperation gizmoOperation_ = GizmoOperation::Translate; ///< 移動・回転・拡縮のうち現在選択している操作。
   GizmoMode gizmoMode_ = GizmoMode::Local;                    ///< ギズモ操作の基準とするローカルまたはワールド座標系。
   std::int32_t selectedVirtualCameraIndex_ = -1;              ///< 一覧で選択中の仮想カメラ番号。-1は未選択。
};

} // namespace GameEngine

#endif // USE_IMGUI
