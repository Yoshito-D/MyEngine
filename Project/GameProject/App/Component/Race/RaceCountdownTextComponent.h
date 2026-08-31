#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
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
   static constexpr const char* kTypeName = "RaceCountdownTextComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レースカウントダウン表示", "Race Countdown Text" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceCountdownTextComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示対象のRaceManagerをIDから解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief レース状態に応じてカウントダウン文字列を更新する
   /// @param deltaTime 回転・拡大・フェードの進行に使うゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 参照先、START文字列、表示アニメーション設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから表示設定を読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void CaptureBaseVisualState();
   void RestoreBaseVisualState(
      GameEngine::UITextComponent& text,
      GameEngine::TransformComponent& transform);
   void ApplyAnimation(
      GameEngine::UITextComponent& text,
      GameEngine::TransformComponent& transform);

   std::string raceManagerId_;
   std::string startText_ = "START";
   RaceManagerComponent* raceManager_ = nullptr;
   std::string displayedText_;
   float animationElapsed_ = 0.0f;
   float rotationDuration_ = 0.45f;
   float fadeDuration_ = 0.45f;
   float fadeEndScale_ = 1.6f;
   GameEngine::Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };
   GameEngine::Vector3 baseEuler_ = {};
   GameEngine::Quaternion baseRotationQuaternion_ = GameEngine::Quaternion::Identity();
   float baseOpacity_ = 1.0f;
   bool baseUsesQuaternion_ = false;
   bool hasBaseVisualState_ = false;
};

} // namespace App
