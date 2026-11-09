#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <string>

namespace App {
/// @brief 共通UIサンプルの操作接続と、ゲーム時間・UI時間の比較表示。
class UISampleComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "UISampleComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"共通UIサンプル", "UI Foundation Sample"};

   /// @brief 保存と検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 時計の種類に応じた更新区分を返す。
   GameEngine::ObjectUpdateDomain GetUpdateDomain() const override;
   /// @brief 画面内キーから操作対象を検索し、ゲーム固有イベントを接続する。
   void OnSceneLoaded(GameEngine::SceneWorld& world) override;
   /// @brief 対応する時計を秒単位で進め、停止の違いを表示する。
   void Update(float deltaTime) override;

   /// @brief シーン破棄時にWorldへの参照を解除する。
   void OnDetach() override { world_ = nullptr; }

   /// @brief 時計の区分だけを保存する。
   nlohmann::json Serialize() const override;
   /// @brief 時計の区分を復元する。経過時間は初期化する。
   void Deserialize(const nlohmann::json& data) override;

private:
   void PlayUIAudio();
   GameEngine::SceneWorld* world_ = nullptr;
   bool uiClock_ = false;
   float elapsed_ = 0.0f;
};
}
