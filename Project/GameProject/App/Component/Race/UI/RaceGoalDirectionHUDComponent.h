#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <string>

namespace GameEngine {
class Object;
class Model;
class UIModelComponent;
}

namespace App {

class RaceManagerComponent;

/// @brief プレイヤーからゴールへの方向を3D矢印で示す
class RaceGoalDirectionHUDComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDirectionHUDComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"ゴール方向3D矢印", "Goal Direction 3D Arrow"};

   /// @brief コンポーネント型名を取得する
   /// @return RaceGoalDirectionHUDComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース、プレイヤー、ゴールへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief プレイヤーとゴールの位置から3D矢印の向きを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 課題の進行に応じて誘導表示を切り替える。空のraceManagerIdではレース状態を参照しない。
   void SetGuidanceEnabled(bool enabled);

   /// @brief 3D矢印の参照設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから3D矢印の参照設定を読み込む
   /// @param data 3D矢印設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief HUD設定と参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void SetVisible(bool visible);

   std::string raceManagerId_ = "Object:RaceManager";         ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string playerObjectId_ = "Model:Player";              ///< 操作対象プレイヤーの安定Entity ID。
   std::string goalObjectId_ = "Model:Goal";                  ///< ゴール地点の安定Entity ID。
   RaceManagerComponent* raceManager_ = nullptr;              ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   GameEngine::Object* playerObject_ = nullptr;               ///< 参照解決済みプレイヤーObjectへの非所有参照。
   GameEngine::Object* goalObject_ = nullptr;                 ///< 参照解決済みゴールObjectへの非所有参照。
   GameEngine::Model* arrowModel_ = nullptr;                  ///< ゴール方向を示す矢印モデルへの非所有参照。
   GameEngine::UIModelComponent* uiModelComponent_ = nullptr; ///< 矢印モデルの画面上配置を制御するUI部品への参照。
   bool guidanceEnabled_ = true;                              ///< チュートリアルなどから指定する誘導表示の許可状態。
};

} // namespace App
