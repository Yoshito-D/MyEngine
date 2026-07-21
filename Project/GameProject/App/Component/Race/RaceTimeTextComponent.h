#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace App {

class RaceManagerComponent;

/// @brief RaceManagerの状態と時間をUITextへ表示する
class RaceTimeTextComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceTimeTextComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レースタイム表示", "Race Time Text" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceTimeTextComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示対象のRaceManagerをIDから解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 現在のレース時間をUITextへ反映する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 参照設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから参照設定を読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_;
   RaceManagerComponent* raceManager_ = nullptr;
};

} // namespace App
