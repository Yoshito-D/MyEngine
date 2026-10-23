#pragma once

#include "GameEngine/Framework/Framework.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Scene/SceneCatalog.h"
#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorSceneSession.h"
#endif
#ifdef USE_IMGUI
#include "GameEngine/Framework/PlayModeController.h"
#endif

/// @brief エンジン基盤とゲーム固有のシーン遷移を結び付けるアプリケーション本体
class Game : public GameEngine::Framework {
public:
   /// @copydoc GameEngine::Framework::Initialize
   void Initialize() override;

   /// @copydoc GameEngine::Framework::Update
   void Update() override;

   /// @copydoc GameEngine::Framework::Draw
   void Draw() override;

   /// @copydoc GameEngine::Framework::Finalize
   void Finalize() override;

   /// @copydoc GameEngine::Framework::EndFrame
   void EndFrame() override;

private:
   void CheckSceneFailure();
   bool fatalSceneFailureReported_ = false; ///< 致命的なシーン読み込み失敗をユーザーへ通知済みかどうか。
   GameEngine::SceneCatalog sceneCatalog_;  ///< 登録シーンの定義と起動シーン設定。
   /// @brief 現在シーンの所有権と安全な切り替えを管理する所有オブジェクト。
   std::unique_ptr<GameEngine::SceneManager> sceneManager_ = nullptr;
#ifdef USE_IMGUI
   /// @brief 編集・プレイ・一時停止の遷移を管理する所有オブジェクト。
   std::unique_ptr<GameEngine::PlayModeController> playModeController_ = nullptr;
   /// @brief 現在シーンの選択・ギズモ・編集状態を管理する所有セッション。
   std::unique_ptr<GameEngine::EditorSceneSession> editorSession_;
   bool recoveryFromPlay_ = false; ///< 置換失敗の復旧元がプレイ開始前の保存状態かどうか。
   bool recoveryWasDirty_ = false; ///< 復旧時に戻す元の未保存変更フラグ。
#endif
};
