#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"

namespace App {

/// @brief 確定した車両イベントを、同じ車両の名前付き音声スロットへ対応させる。
class VehicleAudioController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleAudioController"; ///< 保存と登録に使う型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両サウンド制御", "Vehicle Audio Controller"};

   /// @brief 保存と登録に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 着地品質とドリフト状態が確定した後に音声を更新する。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Presentation; }

   /// @brief シーン読み込み前の通知を既読にし、過去の効果音を再生しない。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 再有効化前の通知を既読にする。
   void OnEnable() override;

   /// @brief 無効化時に車両の効果音を停止する。
   void OnDisable() override;

   /// @brief ジャンプ・着地・ミニターボ発動音を単発再生し、ドリフト音とチャージ完了音をループする。
   /// @param deltaTime 使用しない経過秒数。
   void Update(float deltaTime) override;

   /// @brief 音源設定はAudioSourceComponentが保存するため、空の設定を返す。
   nlohmann::json Serialize() const override { return nlohmann::json::object(); }

   /// @brief 永続設定を持たないため入力を変更しない。
   void Deserialize(const nlohmann::json& data) override { (void)data; }

private:
   void ResetEvents();

   VehicleEventCursor eventCursor_; ///< 他の演出と独立したイベントの既読位置。
};

} // namespace App
