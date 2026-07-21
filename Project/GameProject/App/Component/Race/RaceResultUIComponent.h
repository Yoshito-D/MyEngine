#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace App {

class RaceManagerComponent;

/// @brief ゴールタイム、ベスト3、選択中のリスタート項目をUITextへ表示する
class RaceResultUIComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceResultUIComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レース結果UI", "Race Result UI" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceResultUIComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示対象のRaceManagerをIDから解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 結果表示とリスタート入力を更新する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief RaceManager参照をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからRaceManager参照を読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   std::string BuildResultText() const;

   std::string raceManagerId_;
   RaceManagerComponent* raceManager_ = nullptr;
};

} // namespace App
