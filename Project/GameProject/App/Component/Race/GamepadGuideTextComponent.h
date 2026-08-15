#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace App {

class CharacterJump;
class RaceManagerComponent;

/// @brief ゲームパッドの操作ガイドをリザルト以外でUITextへ表示する
class GamepadGuideTextComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "GamepadGuideTextComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "ゲームパッド操作ガイド", "Gamepad Guide Text" };

   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示条件に使用するRaceManagerを解決する
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief リザルト中だけ操作ガイドを非表示にする
   void Update(float deltaTime) override;

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_;
   std::string playerObjectId_;
   RaceManagerComponent* raceManager_ = nullptr;
   CharacterJump* characterJump_ = nullptr;
};

} // namespace App
