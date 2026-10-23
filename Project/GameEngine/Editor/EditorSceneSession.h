#pragma once

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Editor/Camera/CameraEditor.h"
#include "GameEngine/Scene/Camera/DebugCamera.h"
#include "GameEngine/Scene/Scene.h"

namespace GameEngine {

/// @brief 現在のシーンに接続するエディタ専用カメラとビューポート操作を所有する。
class EditorSceneSession final {
public:
   /// @brief 構築または復旧したシーンへ編集機能を接続する。
   void Attach(Scene& scene, const nlohmann::json& data, bool preserve);

   /// @brief シーンを解放する前に、編集機能が保持する非所有参照を切り離す。
   void Detach();

   /// @brief シーンとの接続を解除してからエディタ資源を破棄する。
   ~EditorSceneSession();

   /// @brief 再生開始時の選択・編集履歴を保持し、再生中の操作用コンテキストへ切り替える。
   void BeginPlay();

   /// @brief 次回接続時に、再生開始時の編集コンテキストを安定IDで再接続する。
   void RestoreEditingStateOnAttach();

   /// @brief デバッグ入力と、ゲーム停止中の描画用データを更新する。
   void Update();

   /// @brief カメラの編集画面とデバッグ形状を描画する。
   void Draw();

   /// @brief 現在の編集コンテキストを取得する。未接続の場合はnullptrを返す。
   EditorSceneContext* GetContext() const { return scene_ ? context_.get() : nullptr; }

   /// @brief カメラ編集機能を取得する。
   CameraEditor* GetCameraEditor() const { return cameraEditor_.get(); }

   /// @brief インスペクターにビューポートの操作設定を描画する。
   void DrawGizmoInspectorControls();

   /// @brief 選択対象を操作し、ドラッグ一回につき一つのUndo操作として記録する。
   void DrawTransformGizmo(float x, float y, float width, float height);

   /// @brief 検証済みアセットの配置を編集コマンドとして受け付ける。
   void AcceptViewportAssetDrop();

   /// @brief ビューポート上への投影位置からシーンのObjectを選択する。
   void HandleViewportClickSelection(float x, float y, float width, float height);

private:
   using GizmoOperation = EditorSceneContext::GizmoOperation;
   using GizmoMode = EditorSceneContext::GizmoMode;
   bool HasTransformChanged(const Transform& lhs, const Transform& rhs) const;
   void SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object);
   void SubmitParticleTransformIfNeeded(const Transform& before, const Transform& after, ParticleSystem* particle);
   void UpdateEditorLightProxies(float deltaTime);
   void DrawEditorComponentDebug();
   void LoadDebugCameraState();
   void SaveDebugCameraState() const;
   std::filesystem::path GetDebugCameraStateFilePath() const;
   Scene* scene_ = nullptr;
   SceneDefinition definition_;
   std::unique_ptr<EditorSceneContext> context_;
   std::unique_ptr<EditorSceneContext> editingContext_;
   bool restoreEditingState_ = false;
   std::unique_ptr<DebugCamera> debugCamera_;
   std::unique_ptr<CameraEditor> cameraEditor_;
   bool isDebugCameraActive_ = false;
   GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
   GizmoMode gizmoMode_ = GizmoMode::Local;
   bool isManipulating_ = false;
   Object* manipulatingObject_ = nullptr;
   Transform transformBeforeManipulation_{};
   bool isManipulatingParticleSystem_ = false;
   ParticleSystem* manipulatingParticleSystem_ = nullptr;
   Transform particleTransformBeforeManipulation_{};
};
}
#endif
