#pragma once

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorSceneSession.h"
#include "GameEngine/Framework/PlayModeController.h"
#include <filesystem>

namespace GameEngine {

struct SceneRecovery;

/// @brief 再生モードと、シーン差し替えをまたいで維持する編集セッションを所有する。
class EditorPlaySession final {
public:
   /// @brief エディターで使用する時間と更新可否。
   struct FrameTiming {
      bool runRuntime;   ///< このフレームで実行時更新を行うか。
      bool audioPaused;  ///< 編集・一時停止による音声停止が必要か。
      float uiSeconds;   ///< UIに適用する時間（秒）。
      float gameSeconds; ///< ゲームに適用する時間（秒）。
   };

   /// @brief シーン管理へ編集機能と復旧処理を接続する。
   /// @param scenes このセッションより長く生存するシーン管理。
   /// @param audio 再生モードと音声の同期に使用するサービス。
   /// @param stateFile 次回の編集開始位置を保存するファイル。
   EditorPlaySession(SceneManager& scenes, Audio& audio, std::filesystem::path stateFile);

   /// @brief シーン管理への通知とエンジンからの非所有参照を解除する。
   ~EditorPlaySession();

   /// @brief シーン管理に登録した通知先のアドレスを固定するため、コピーを禁止する。
   EditorPlaySession(const EditorPlaySession&) = delete;
   /// @brief 登録済みの通知先と状態の対応を保つため、コピー代入を禁止する。
   EditorPlaySession& operator=(const EditorPlaySession&) = delete;

   /// @brief 前回の編集シーンを返す。保存がなければカタログの起動シーンを返す。
   std::string ResolveInitialScene();

   /// @brief 次回の編集開始用に、現在シーンの名前を保存する。
   void SaveCurrentScene() const;

   /// @brief 保留中のプレイ・停止・ステップ要求を処理し、各更新の時間を返す。
   /// @param realSeconds フレームの実時間（秒）。
   FrameTiming BeginFrame(float realSeconds);

   /// @brief 選択・デバッグカメラなどの編集機能を更新する。
   void Update();

   /// @brief 現在の編集セッションを描画する。
   void Draw();

private:
   SceneRecovery CaptureRecovery(const Scene& scene);
   void RestoreEditorState(Scene& scene, const SceneSnapshot& snapshot, bool fromPlay, bool wasDirty);

   SceneManager& scenes_;
   std::filesystem::path stateFile_;
   PlayModeController playMode_;
   EditorSceneSession session_;
};

} // namespace GameEngine
#endif
