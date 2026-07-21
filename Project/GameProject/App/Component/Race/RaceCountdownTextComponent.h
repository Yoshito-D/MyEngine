#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

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
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 参照先とSTART文字列をJSONへ保存する
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
   std::string raceManagerId_;
   std::string startText_ = "START";
   RaceManagerComponent* raceManager_ = nullptr;
};

} // namespace App
