#pragma once
#include "IScene.h"
#include <EngineContext.h>
#include <DebugCamera.h>
#include "SceneFade.h"

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

protected:
   /// @brief デバッグカメラの更新
   void UpdateDebugCamera();

   /// @brief フェードを設定（派生クラスで独自のSceneFadeを設定可能）
   /// @param fade フェードオブジェクト（所有権を移譲）
   void SetFade(std::unique_ptr<SceneFade> fade);

   /// @brief デフォルトのフェードを作成
   /// @param fadeDuration フェード時間（秒）
   /// @param fadeColor フェードカラー
   void CreateDefaultFade(float fadeDuration = 1.0f, uint32_t fadeColor = 0x000000ff);

#ifdef USE_IMGUI
   std::unique_ptr<DebugCamera> debugCamera_ = nullptr;
   bool isDebugCameraActive_ = false;
   Transform mainCameraPrevTransform_{};
#endif

   std::unique_ptr<Camera> mainCamera_ = nullptr;
   std::unique_ptr<Camera> uiCamera_ = nullptr;
   std::unique_ptr<SceneFade> sceneFade_ = nullptr;

   // 静的メンバー
   static inline std::string sNextSceneName_ = "";
   static inline std::string sPendingSceneName_ = "";
   static inline bool sIsWaitingForFadeOut_ = false;
   static inline BaseScene* sCurrentScene_ = nullptr;  // 現在のシーンインスタンス

   bool isFinished_ = false;
};
}
