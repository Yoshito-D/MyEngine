#pragma once
#include "GameEngine/Scene/Scene.h"
#include "GameEngine/Scene/SceneCatalog.h"
#include <functional>
#include <optional>

namespace GameEngine {

/// @brief 予約した遷移も含めた、直近のシーン操作の結果。
enum class SceneLoadStatus {
   Succeeded,
   Rejected,
   Recovered,
   Fatal
};

/// @brief エディタに表示できる処理結果と診断メッセージ。
struct SceneLoadResult {
   SceneLoadStatus status = SceneLoadStatus::Succeeded;
   std::string message;

   /// @brief 処理が正常に完了した場合だけtrueへ変換する。
   explicit operator bool() const { return status == SceneLoadStatus::Succeeded; }
};

/// @brief 現在のシーンと遷移予約、フェード状態を一括管理する。
class SceneManager final {
public:
   /// @brief アプリケーションとエディタで共有するカタログを使用する。
   explicit SceneManager(SceneCatalog& catalog);

   /// @brief エンジンの各機能を破棄する前に、現在のシーンを解放する。
   ~SceneManager();

   /// @brief 最初のフレームより前に、起動シーンを読み込む。
   SceneLoadResult LoadInitialScene(const std::string& name);

   /// @brief 最初の有効な遷移を予約し、遷移中の追加要求を拒否する。
   bool RequestSceneChange(const std::string& name);

   /// @brief 現在のシーン名をカタログで再解決し、登録先から再読込する操作を予約する。
   bool RequestReload();

   /// @brief 安全なフレーム境界で、ファイルを読まずメモリーのスナップショットから復元する。
   SceneLoadResult RestoreSnapshot(const SceneSnapshot& snapshot);

   /// @brief シーンの実体を破棄せず、実時間でフェードを進める。
   void EditorUpdate();

   /// @brief 現在のWorldのランタイム更新を実行する。
   void RuntimeUpdate();

   /// @brief 描画が完了したフレーム境界で、予約したシーン切替を確定する。
   void CheckSceneChange();

   /// @brief すべての遷移予約を取り消し、暗転表示を初期化する。
   void CancelTransition();

   /// @brief 編集機能との接続を解除してから現在のシーンを破棄する。
   void Finalize();

   /// @brief 現在のシーンを取得する。
   Scene* GetCurrentScene() const { return currentScene_.get(); }

   /// @brief 切替が確定したシーンの名前を取得する。
   std::string GetCurrentSceneName() const;

   /// @brief 共有カタログを取得する。
   SceneCatalog& GetCatalog() { return catalog_; }

   /// @brief 即時操作と予約操作を含む、直近の処理結果を取得する。
   const SceneLoadResult& GetLastResult() const { return lastResult_; }

   /// @brief エディタの型へ依存せず、シーン切替前後と復旧元取得の通知を接続する。
   void SetLifecycleCallbacks(std::function<void()> before, std::function<void(const SceneSnapshot&, bool)> after,
      std::function<SceneSnapshot()> recovery);

private:
   bool ReadScene(const std::string& name, SceneSnapshot& snapshot);
   SceneLoadResult Replace(const SceneSnapshot& snapshot);
   SceneLoadResult Report(SceneLoadStatus status, std::string message);
   void BeginFadeIn();
   SceneCatalog& catalog_;
   std::unique_ptr<Scene> currentScene_;
   std::optional<SceneSnapshot> pending_;
   enum class TransitionState {
      Idle,
      FadingOut,
      FadingIn
   };
   TransitionState transitionState_ = TransitionState::Idle;
   float elapsed_ = 0.0f;
   float opacity_ = 0.0f;
   bool commitReady_ = false;
   bool replacing_ = false;
   SceneLoadResult lastResult_;
   std::function<void()> beforeReplace_;
   std::function<void(const SceneSnapshot&, bool)> afterReplace_;
   std::function<SceneSnapshot()> captureRecovery_;
   static constexpr float kTransitionDuration = 0.4f;
};
}
