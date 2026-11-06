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
   /// @brief エンジン基盤とシーン管理を初期化し、起動シーンと編集セッションを接続する。
   /// @details シーンカタログの読み込みに失敗した場合は通知して終了を要求する。
   void Initialize() override;

   /// @brief 共有設定を同期し、プレイ状態に応じてエンジン・シーン・編集セッションと音声を更新する。
   /// @details エディターの停止中はランタイム更新を進めず、編集用更新を継続する。
   void Update() override;

   /// @brief エディター描画を行い、有効なプレイヤーの影情報を現在のカメラへ提出する。
   /// @details シーンまたは描画カメラがない場合は、影の描画情報を提出しない。
   void Draw() override;

   /// @brief シーンを終了し、編集用サービスとエンジン基盤を依存関係の逆順に解放する。
   /// @details エディタービルドでは次回起動用の編集シーン名を保存する。
   void Finalize() override;

   /// @brief フレーム描画を完了し、遅延削除と予約済みのシーン切り替えを確定する。
   /// @details 描画中の参照を無効化しないよう、シーン実体の変更はフレーム末尾で行う。
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
