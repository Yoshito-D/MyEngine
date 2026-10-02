#pragma once
#include "GameEngine/Audio/Audio.h"
#include <string>

namespace GameEngine {
/// @brief シーンが所有し、実行時だけ使用するBGM設定。
struct BgmRequest {
   std::string clipAssetId;
   std::shared_ptr<const SoundClip> clip;
   float volume = 1.0f;
   bool loop = true;
   float fadeInSeconds = 0.0f;
   float fadeOutSeconds = 0.0f;
   bool continueAcrossScenes = false;
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
      AudioHandle handle;
      std::string assetId;
      uint64_t owner = 0;
      float gain = 1.0f;
      float start = 1.0f, target = 1.0f, duration = 0.0f, elapsed = 0.0f;
      float fadeOutSeconds = 0.0f;
      bool continueAcrossScenes = false;
   };
   void Fade(Track& track, float target, float seconds);
   void Release(Track& track);
   Audio& audio_;
   Track current_, outgoing_;
   uint64_t nextOwner_ = 0;
   bool sceneChanging_ = false;
};
}
