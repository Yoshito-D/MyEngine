#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/VectorMath.h"
#include <string>

namespace GameEngine {
class TransformComponent;
class UITextComponent;
}

namespace App {

class RaceManagerComponent;

/// @brief レース開始前の3・2・1とSTARTをUITextへ表示する
class RaceCountdownTextComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceCountdownTextComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"レースカウントダウン表示", "Race Countdown Text"};

   /// @brief コンポーネント型名を取得する
   /// @return RaceCountdownTextComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示対象のRaceManagerをIDから解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief レース状態に応じてカウントダウン文字列を更新する
   /// @param deltaTime 回転・拡大・フェードの進行に使うゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 参照先、START文字列、表示アニメーション設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから表示設定を読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief 参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   void CaptureBaseVisualState();
   void RestoreBaseVisualState(GameEngine::UITextComponent& text, GameEngine::TransformComponent& transform);
   void ApplyAnimation(GameEngine::UITextComponent& text, GameEngine::TransformComponent& transform);

   std::string raceManagerId_;                          ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string startText_ = "START";                    ///< カウントダウン終了時に表示する文字列。
   RaceManagerComponent* raceManager_ = nullptr;        ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   std::string displayedText_;                          ///< 最後にUIへ反映した文字列。重複更新を避けるために保持する。
   float animationElapsed_ = 0.0f;                      ///< 現在のUI演出を開始してからの経過時間（秒）。
   float rotationDuration_ = 0.45f;                     ///< カウント表示の回転演出に使う時間（秒）。
   float fadeDuration_ = 0.45f;                         ///< UIをフェードさせる時間（秒）。
   float fadeEndScale_ = 1.6f;                          ///< フェード完了時の基準サイズに対する拡大率。
   GameEngine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f}; ///< 演出開始前に記録した基準スケール。
   GameEngine::Vector3 baseEuler_ = {};                 ///< 演出開始前に記録した基準Euler角（ラジアン）。
   /// @brief 演出開始前に記録した基準姿勢。
   GameEngine::Quaternion baseRotationQuaternion_ = GameEngine::Quaternion::Identity();
   float baseOpacity_ = 1.0f;        ///< 演出で上書きする前の基準不透明度。
   bool baseUsesQuaternion_ = false; ///< 基準姿勢の保存形式がQuaternionだったかどうか。
   bool hasBaseVisualState_ = false; ///< 演出前のサイズ・姿勢・透明度を記録済みかどうか。
};

} // namespace App
