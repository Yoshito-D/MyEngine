#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/UI/Text/TextTypes.h"
#include <string_view>

namespace App {

/// @brief ゴールを示す3D矢印を、ミニマップの距離表示に並べる。
class RaceGoalDirectionHUDComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDirectionHUDComponent"; ///< 保存用の型名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"ゴール方向3D矢印", "Goal Direction 3D Arrow"};

   /// @brief 保存用のコンポーネント型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 距離文字列の直前に、ゲームカメラから見たゴール方向を表示する。
   /// @param directionToGoal プレイヤーからゴールへのワールド変位（m）。
   /// @param captionOffset 画面右下から距離表示の下端中央への差分（UIピクセル、Yは下向き）。
   /// @param caption ミニマップの距離文字列。
   /// @param style 距離文字列のフォントと表示設定。
   void ShowBesideDistance(const GameEngine::Vector3& directionToGoal, GameEngine::Vector2 captionOffset,
      std::string_view caption, const GameEngine::TextStyle& style);

   /// @brief この矢印の描画を止める。
   void Hide();

   /// @brief 無効化されたとき、矢印を非表示にする。
   void OnDisable() override;

   /// @brief 保存設定を持たないため空のJSONオブジェクトを返す。
   nlohmann::json Serialize() const override;

   /// @brief 矢印の表示先と参照先はミニマップと共有するため、空の設定を読み込む。
   /// @param data 矢印の設定を表す空のJSONオブジェクト。
   void Deserialize(const nlohmann::json& data) override;

private:
   void SetVisible(bool visible);
};

} // namespace App
