#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {
class Object;
class UITextComponent;
}

namespace App {

/// @brief メニュー項目の参照と基準姿勢を保持し、選択・決定・非表示の演出を管理する。
class MenuSelectionAnimationComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "MenuSelectionAnimationComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"メニュー選択演出", "Menu Selection Animation"};

   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief シーンに保存されたスケールと不透明度を取り直し、入力待ちまで項目を非表示にする。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 未解決または変更された項目の参照と基準姿勢を取得する。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 選択中の脈動、選択切替、決定時の拡大・フェードを更新する。
   void Update(float deltaTime) override;

   /// @brief 選択項目を変更する。同じ項目への通知では再生位置を維持する。
   /// @param optionIndex 保存された項目一覧の添字。
   /// @param animateSwitch 縮小状態からの切替演出を再生する場合はtrue。
   /// @return TextとTransformを持つ有効な項目を選択できた場合はtrue。
   bool Select(std::int32_t optionIndex, bool animateSwitch = true);

   /// @brief 選択中の実際のスケールから決定演出を開始する。
   /// @return 有効な選択項目があり、決定演出を開始できた場合はtrue。
   bool ConfirmSelection();

   /// @brief 全項目の文字列を消し、スケールと不透明度を基準値へ戻す。
   void Hide();

   /// @brief 基準姿勢を取得済みで、現在もTextとTransformを持つ項目か判定する。
   bool IsOptionAvailable(std::size_t optionIndex) const;

   /// @brief 項目の表示内容を画面側で設定するためのTextを取得する。
   /// @return 項目が利用できない場合はnullptr。
   GameEngine::UITextComponent* GetOptionText(std::size_t optionIndex) const;

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   enum class State {
      Hidden,
      Selecting,
      Reacting
   };

   struct OptionVisual {
      std::string objectId;                               ///< 編集や参照の対象オブジェクトを識別する安定Entity ID。
      GameEngine::Vector3 baseScale = {1.0f, 1.0f, 1.0f}; ///< 演出開始前に記録した基準スケール。
      float baseOpacity = 1.0f;                           ///< 演出で上書きする前の基準不透明度。
      bool captured = false;                              ///< 演出前の基準サイズと透明度を取得済みかどうか。
   };

   GameEngine::Object* FindOptionObject(std::size_t optionIndex) const;
   float EvaluateSelectionScale(float deltaTime);
   void ApplySelection(float scale);
   void ApplyReaction();
   void ValidateSettings();

   std::vector<OptionVisual> options_; ///< 選択演出を適用するメニュー項目と基準表示状態。
   float pulseDuration_ = 0.9f;        ///< 選択中の拡縮を繰り返す周期（秒）。
   float pulseScale_ = 1.1f;           ///< 選択中の拡縮演出で使う最大スケール倍率。
   float switchDuration_ = 0.16f;      ///< 選択項目の変更演出に使う時間（秒）。
   float switchScale_ = 0.82f;         ///< 選択が切り替わるときに一時適用するスケール倍率。
   float reactionDuration_ = 0.4f;     ///< 選択確定後の反応演出に使う時間（秒）。
   float reactionEndScale_ = 1.2f;     ///< 選択確定演出の終了時に使うスケール倍率。
   State state_ = State::Hidden;       ///< 現在の処理段階または実行状態。
   std::int32_t selectedOption_ = -1;  ///< 現在選択中のメニュー項目番号。負値は未選択。
   float elapsed_ = 0.0f;              ///< 処理を開始してからの経過時間（秒）。
   bool switching_ = false;            ///< 選択項目の切り替え演出が進行中かどうか。
   bool skipAdvance_ = false;          ///< 演出を開始したフレームの時間加算を一度だけ省略する状態。
   GameEngine::Vector3 reactionStartScale_ = {1.0f, 1.0f, 1.0f}; ///< 選択確定演出を開始したときのスケール。
};

} // namespace App
