#pragma once

#include "Component/IObjectComponent.h"
#include <cstddef>
#include <cstdint>

namespace GameEngine {

/// @brief UITextComponentを先頭から1文字ずつ表示するコンポーネント
class TypewriterComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "TypewriterComponent";
   static constexpr ComponentDisplayName kDisplayName{ "タイプライター", "Typewriter" };

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief アタッチ時に自動再生設定を適用する
   void OnAttach() override;

   /// @brief 有効化時に自動再生設定を適用する
   void OnEnable() override;

   /// @brief 経過時間に応じて表示文字数を更新する
   /// @param deltaTime 経過秒
   void Update(float deltaTime) override;

   /// @brief 現在位置から再生を続行する
   void Play();

   /// @brief 現在の表示文字数で一時停止する
   void Pause();

   /// @brief 文字を隠して先頭から再生する
   void Restart();

   /// @brief すべての文字を即時表示する
   void Complete();

   /// @brief 再生中かを取得する
   /// @return 再生中ならtrue
   bool IsPlaying() const { return playing_; }

   /// @brief 設定をJSON化する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief エディタのインスペクターを描画する
   void DrawInspector() override;
#endif

   /// @brief 文字送り速度と再生設定
   float glyphsPerSecond = 20.0f;
   float delay = 0.0f;
   bool playOnEnable = true;
   bool loop = false;
   bool restartOnTextChange = true;

private:
   size_t CountGlyphs() const;

   float elapsed_ = 0.0f;
   bool playing_ = true;
   uint64_t observedTextRevision_ = 0;
};

} // namespace GameEngine
