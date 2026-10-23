#pragma once

#include "GameEngine/Audio/Audio.h"
#include <string>

namespace GameEngine {

/// @brief シーンが所有し、実行時だけ使用するBGM設定。
struct BgmRequest {
   std::string clipAssetId;               ///< 再生する音声クリップの永続アセットID。
   std::shared_ptr<const SoundClip> clip; ///< 再生対象の音声またはアニメーションクリップへの参照。
   float volume = 1.0f;                   ///< 個別の音量倍率。
   bool loop = true;                      ///< 終端到達後に再生を繰り返すかどうか。
   float fadeInSeconds = 0.0f;            ///< 音量を立ち上げる時間（秒）。
   float fadeOutSeconds = 0.0f;           ///< 音量を下げて停止する時間（秒）。
   bool continueAcrossScenes = false;     ///< シーン切り替え後も再生を継続するかどうか。
};

/// @brief シーンのアドレスに依存しない所有者トークンで、最大2つのBGMボイスを管理する。
class BgmPlayer {
public:
   /// @brief 所有元のオーディオサービスへ紐付ける。
   explicit BgmPlayer(Audio& audio) : audio_(audio) {}

   /// @brief コンポーネントの1回の有効化に対して一意な所有権トークンを発行する。
   uint64_t NewOwnerToken() { return ++nextOwner_; }

   /// @brief トラックを要求する。同じアセットは明示的に再起動しない限り継続する。
   void Request(uint64_t owner, const BgmRequest& request, bool restart = false);

   /// @brief まだアクティブな要求元である場合だけフェードアウトする。
   void ReleaseOwner(uint64_t owner);

   /// @brief 次のシーンを確認するまで、切り替え元シーンのトラックを保持する。
   void BeginSceneChange() { sceneChanging_ = true; }

   /// @brief 新しいBGM要求がなかった場合に、以前のトラックの継続設定を適用する。
   void EndSceneChange(bool hasNewRequest);

   /// @brief 実時間秒でフェードを進める。呼び出し側はゲーム一時停止中に更新を止める。
   void Update(float seconds);

   /// @brief 両方のトラックを停止し、すべての所有者トークンを無効化する。
   void StopImmediate();

   /// @brief 診断とテスト用に現在のトラックIDを取得する。
   const std::string& GetCurrentAssetId() const { return current_.assetId; }
#ifdef MYPROJECT_NON_RELEASE

   /// @brief デバイス不要のテストに限り、トラックの識別情報を公開する。
   AudioHandle GetCurrentHandleForTesting() const { return current_.handle; }
#endif

private:
   struct Track {
      AudioHandle handle;  ///< 世代番号を含めて対象を特定する操作ハンドル。
      std::string assetId; ///< Resources配下のファイルを参照する永続アセットID。
      uint64_t owner = 0;  ///< BGM要求元を識別するトークン。0は所有者なし。
      float gain = 1.0f;   ///< 現在適用している音量倍率。
      /// @brief start: 補間や処理の開始値。target: 補間または参照の対象。duration:
      /// 処理または再生が継続する時間（秒）。elapsed: 処理を開始してからの経過時間（秒）。
      float start = 1.0f, target = 1.0f, duration = 0.0f, elapsed = 0.0f;
      float fadeOutSeconds = 0.0f;       ///< 音量を下げて停止する時間（秒）。
      bool continueAcrossScenes = false; ///< シーン切り替え後も再生を継続するかどうか。
   };

   void Fade(Track& track, float target, float seconds);
   void Release(Track& track);
   Audio& audio_; ///< 再生ハンドルと音量操作を委譲するAudioへの非所有参照。
   /// @brief current_: 現在選択または再生している対象の状態。outgoing_:
   /// 切り替え前のBGMとフェードアウトの進行状態。
   Track current_, outgoing_;
   uint64_t nextOwner_ = 0;     ///< BGM要求元へ次に発行する一意トークン。
   bool sceneChanging_ = false; ///< 旧シーンの解放から新シーンの要求確定までの切り替え中かどうか。
};
}
