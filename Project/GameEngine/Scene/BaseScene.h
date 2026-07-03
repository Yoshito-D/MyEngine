#pragma once
#include "IScene.h"
#include <EngineContext.h>
#include "Camera/DebugCamera.h"
#include "Camera/Core/CinemachineBrain.h"
#include "Editor/Camera/CameraEditor.h"
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

   /// @brief シーンの共通初期化を実行してから派生シーンの初期化フックを呼び出す
   void Initialize() override final;

   /// @brief エディタ更新とランタイム更新を固定順で実行する
   void Update() override final;

   /// @brief エディタ用の共通更新を実行してから派生シーンのエディタ更新フックを呼び出す
   void EditorUpdate();

   /// @brief 派生シーンのランタイム更新フックを呼び出す
   void RuntimeUpdate();

   /// @brief 派生シーンの描画フックを呼び出してから共通エディタ描画を実行する
   void Draw() override final;

   /// @brief 派生シーンの終了フックを呼び出してから共通リソースを破棄する
   void Finalize() override final;

   /// @brief 次のシーン名を取得
   /// @return 次のシーン名
   std::string GetNextSceneName() const override final { return sNextSceneName_; }

   /// @brief 次のシーンを設定（静的メソッド - 現在のシーンのフェードを使用）
   /// @param sceneName 次のシーン名
   static void SetNextSceneName(const std::string& sceneName);

   /// @brief 現在実行中のシーンを取得する
   /// @return 現在実行中のシーン。存在しない場合は nullptr
   static BaseScene* GetCurrentScene() { return sCurrentScene_; }

   /// @brief エディタで読み込むシーンデータ名を設定する
   /// @param sceneName エディタ用シーン名
   void SetEditorSceneName(const std::string& sceneName) { editorSceneName_ = sceneName; }

   /// @brief エディタで読み込むシーンデータ名を取得する
   /// @return エディタ用シーン名
   const std::string& GetEditorSceneName() const { return editorSceneName_; }

#ifdef USE_IMGUI
   /// @brief エディタ用シーンコンテキストを取得する
   /// @return エディタ用シーンコンテキスト。未初期化の場合は nullptr
   EditorSceneContext* GetEditorSceneContext() { return editorSceneContext_.get(); }

   /// @brief エディタ用シーンコンテキストを取得する
   /// @return エディタ用シーンコンテキスト。未初期化の場合は nullptr
   const EditorSceneContext* GetEditorSceneContext() const { return editorSceneContext_.get(); }

   /// @brief カメラエディタを取得する
   /// @return カメラエディタ。未初期化の場合は nullptr
   CameraEditor* GetCameraEditor() { return cameraEditor_.get(); }

   /// @brief カメラエディタを取得する
   /// @return カメラエディタ。未初期化の場合は nullptr
   const CameraEditor* GetCameraEditor() const { return cameraEditor_.get(); }

   /// @brief 必要に応じてエディタ用シーンデータを自動読み込みする
   void LoadEditorSceneIfNeeded();
#endif

protected:
   /// @brief 共通初期化後に派生シーン固有の初期化を行う
   virtual void OnInitialize() {}

   /// @brief 共通エディタ更新後に派生シーン固有のエディタ更新を行う
   virtual void OnEditorUpdate() {}

   /// @brief 派生シーン固有のランタイム更新を行う
   /// @param deltaTime ゲーム用デルタタイム（秒）
   virtual void OnUpdate(float deltaTime) { (void)deltaTime; }

   /// @brief 共通エディタ描画前に派生シーン固有の描画を行う
   virtual void OnDraw() {}

   /// @brief 共通リソース破棄前に派生シーン固有の終了処理を行う
   virtual void OnFinalize() {}

private:

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
