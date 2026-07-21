#pragma once

#include "Object/Component/IObjectComponent.h"
#include <cstddef>
#include <string>

namespace App {

class RaceManagerComponent;

/// @brief TriggerVolumeの侵入をレース進行へ変換するゴール／ゲート
class RaceGateComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief ゲートの役割
   enum class GateType {
      Start,
      Checkpoint,
      Finish,
      StartFinish,
   };

   static constexpr const char* kTypeName = "RaceGateComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レースゴール／ゲート", "Race Goal / Gate" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceGateComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief ゴール判定に必要なTriggerVolumeを自動追加する
   void OnAttach() override;

   /// @brief レース管理コンポーネントへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief トリガーの侵入・退出をレース管理へ通知する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief ゲート設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからゲート設定を読み込む
   /// @param data ゲート設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief ゲート設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_ = "race_manager";
   GateType gateType_ = GateType::Finish;
   size_t checkpointIndex_ = 0;
   RaceManagerComponent* raceManager_ = nullptr;
};

} // namespace App
