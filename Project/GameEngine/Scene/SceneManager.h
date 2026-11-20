#pragma once
#include "GameEngine/Scene/Scene.h"
#include "GameEngine/Scene/SceneCatalog.h"
#include "GameEngine/Utility/StateMachine.h"
#include <chrono>
#include <functional>
#include <optional>

namespace GameEngine {

class AssetManager;
class Audio;
class CameraManager;
class Renderer;

/// @brief 起動・置換・復旧を実行した結果。
enum class SceneLoadStatus {
   Succeeded,
   Rejected,
   Recovered,
   Fatal
};

/// @brief エディタに表示できる処理結果と診断メッセージ。
struct SceneLoadResult {
   SceneLoadStatus status = SceneLoadStatus::Succeeded; ///< 処理結果の成功・拒否・復旧・失敗などの分類。
   std::string message;                                 ///< 処理結果や失敗理由を伝える説明文字列。

   /// @brief 処理が正常に完了した場合だけtrueへ変換する。
   explicit operator bool() const { return status == SceneLoadStatus::Succeeded; }
};

/// @brief 置換一回分の復旧元と、復旧したシーンへ外部の編集状態を戻す処理。
struct SceneRecovery {
   SceneSnapshot snapshot; ///< 旧シーンまたはプレイ前の編集状態。
   std::function<void(Scene&, const SceneSnapshot&)> attach; ///< 復旧したシーンへの接続処理。不要なら空。
};

/// @brief 現在のシーンと遷移予約、フェード状態を一括管理する。
class SceneManager final {
public:
   /// @brief アプリケーションとエディタで共有するカタログを使用する。
   /// @details 共有サービスはマネージャーより長く生存すること。
   SceneManager(SceneCatalog& catalog, AssetManager& assets, Audio& audio, CameraManager& cameras, Renderer& renderer);

   /// @brief エンジンの各機能を破棄する前に、現在のシーンを解放する。
   ~SceneManager();

   /// @brief 最初のフレームより前に、起動シーンを読み込む。
   SceneLoadResult LoadInitialScene(const std::string& name);

   /// @brief 最初の有効な遷移を予約し、遷移中の追加要求を拒否する。
   SceneRequestStatus RequestSceneChange(const std::string& name);

   /// @brief 現在のシーン名をカタログで再解決し、登録先から再読込する操作を予約する。
   SceneRequestStatus RequestReload();

   /// @brief 安全なフレーム境界で、ファイルを読まずメモリーのスナップショットから復元する。
   SceneLoadResult RestoreSnapshot(const SceneSnapshot& snapshot);

   /// @brief シーンの実体を破棄せず、実時間でフェードを進める。
   void UpdateTransitionFade();

   /// @brief 描画が完了したフレーム境界で、予約したシーン切替を確定する。
   void CommitPendingSceneChange();

   /// @brief すべての遷移予約を取り消し、暗転表示を初期化する。
   void CancelTransition();

   /// @brief 編集機能との接続を解除してから現在のシーンを破棄する。
   void Finalize();

   /// @brief 現在のシーンを取得する。
   Scene* GetCurrentScene() const { return currentScene_.get(); }

   /// @brief 切替が確定したシーンの名前を取得する。
   std::string GetCurrentSceneName() const;

   /// @brief 入場・退出のフェード、または予約されたシーン切り替えが進行中か返す。
   bool IsSceneTransitioning() const;

   /// @brief 共有カタログを取得する。
   SceneCatalog& GetCatalog() { return catalog_; }

   /// @brief 起動・置換・復旧の直近の実行結果を取得する。
   const SceneLoadResult& GetLastResult() const { return lastResult_; }

   /// @brief 直近の予約エラー。予約受付またはシーン置換の完了時に消去する。
   const std::string& GetLastRequestError() const { return lastRequestError_; }

   /// @brief エディタの型へ依存せず、シーン切替前後と復旧元取得の通知を接続する。
   void SetLifecycleCallbacks(std::function<void()> detach, std::function<void(Scene&, const SceneSnapshot&)> attach,
      std::function<SceneRecovery(const Scene&)> captureRecovery);

private:
   enum class TransitionState {
      Idle,
      FadingOut,
      WaitingForCommit,
      PreparingFadeIn,
      FadingIn
   };

   void ConfigureTransitionStates();
   void UpdateFade(float deltaTime, bool fadingOut);
   bool ReadScene(const std::string& name, SceneSnapshot& snapshot, std::string& error);
   SceneLoadResult Replace(const SceneSnapshot& snapshot);
   SceneLoadResult Report(SceneLoadStatus status, std::string message);
   void BeginFadeIn();
   AssetManager& assets_;
   Audio& audio_;
   CameraManager& cameras_;
   Renderer& renderer_;
   SceneCatalog& catalog_;                           ///< 共有カタログ。
   std::unique_ptr<Scene> currentScene_;             ///< 現在実行・編集している所有シーン。
   std::optional<SceneSnapshot> pending_;            ///< フレーム境界で適用する読み込み済みのシーン切り替え要求。
   StateMachine<TransitionState> transitionMachine_; ///< 暗転・シーン置換待ち・復帰の入場と更新を管理する。
   std::chrono::steady_clock::time_point transitionUpdateTime_; ///< フェード開始以降の時間だけを計測する基準時刻。
   float elapsed_ = 0.0f;                                       ///< 処理を開始してからの経過時間（秒）。
   float opacity_ = 0.0f;                                       ///< 影の濃さ（0～1）。
   bool replacing_ = false;                                     ///< シーンの構築・復旧中で再入を拒否するかどうか。
   std::string lastRequestError_;                              ///< 予約の拒否理由。
   SceneLoadResult lastResult_;                                 ///< 起動・置換・復旧の直近の実行結果。
   std::function<void()> beforeReplace_;                        ///< 旧シーンを破棄する前に編集状態などを切り離す通知。
   std::function<void(Scene&, const SceneSnapshot&)> afterReplace_; ///< 新規シーンを構築した後の接続通知。
   std::function<SceneRecovery(const Scene&)> captureRecovery_;               ///< 置換失敗時の復元元となる保存状態を取得する処理。
   static constexpr float kTransitionDuration = 0.4f;             ///< シーン遷移の暗転・復帰にそれぞれ使う時間（秒）。
};
}
