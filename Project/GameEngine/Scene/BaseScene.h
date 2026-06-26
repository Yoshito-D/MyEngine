#pragma once
#include "IScene.h"
#include <EngineContext.h>
#include "Camera/DebugCamera.h"
#include "Camera/Core/CinemachineBrain.h"
#include "Camera/Editor/CameraEditor.h"
#include <filesystem>
#ifdef USE_IMGUI
#include "Editor/EditorSceneContext.h"
#endif

namespace GameEngine {
/// @brief 基底シーンクラス
class BaseScene : public IScene {
public:
   /// @brief デストラクタ
   virtual ~BaseScene() = default;

   /// @brief シーンの初期化
   virtual void Initialize() override;

   /// @brief シーンの更新
   virtual void Update() override;

   /// @brief シーンの描画
   virtual void Draw() override;

   /// @brief シーンの終了処理
   virtual void Finalize() override;

   /// @brief 次のシーン名を取得
   /// @return 次のシーン名
   virtual std::string GetNextSceneName() const override { return sNextSceneName_; }

   /// @brief 次のシーンを設定（静的メソッド - 現在のシーンのフェードを使用）
   /// @param sceneName 次のシーン名
   static void SetNextSceneName(const std::string& sceneName);

   static BaseScene* GetCurrentScene() { return sCurrentScene_; }

   void SetEditorSceneName(const std::string& sceneName) { editorSceneName_ = sceneName; }
   const std::string& GetEditorSceneName() const { return editorSceneName_; }

#ifdef USE_IMGUI
   EditorSceneContext* GetEditorSceneContext() { return editorSceneContext_.get(); }
   const EditorSceneContext* GetEditorSceneContext() const { return editorSceneContext_.get(); }
   void LoadEditorSceneIfNeeded();
#endif

protected:
   /// @brief デバッグカメラの更新
   void UpdateDebugCamera();

#ifdef USE_IMGUI
   void LoadDebugCameraState();
   void SaveDebugCameraState() const;
   std::filesystem::path GetDebugCameraStateFilePath() const;

   std::unique_ptr<DebugCamera> debugCamera_ = nullptr;
   std::unique_ptr<CameraEditor> cameraEditor_ = nullptr;
   std::unique_ptr<EditorSceneContext> editorSceneContext_ = nullptr;
   bool isDebugCameraActive_ = false;
#endif

   // 静的メンバー
   static inline std::string sNextSceneName_ = "";
   static inline std::string sPendingSceneName_ = "";
   static inline bool sIsWaitingForFadeOut_ = false;
   static inline BaseScene* sCurrentScene_ = nullptr;  // 現在のシーンインスタンス

   bool isFinished_ = false;
   std::string editorSceneName_ = "Scene";
};
}
