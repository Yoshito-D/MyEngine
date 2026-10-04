#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"

namespace App {

/// @brief 重力Up方向へジャンプ初速を付与するコンポーネント
class CharacterJump final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterJump";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "キャラクタージャンプ", "Character Jump" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 未ジャンプ時のみジャンプ速度を付与する
   /// @param gravityUp 現在の重力Up方向
   void Jump(const GameEngine::Vector3& gravityUp);

   /// @brief ジャンプ中かどうかを返す
   bool IsJumping()    const { return isJumping_; }

   /// @brief 着地通知を受けてジャンプ状態を解除する
   void NotifyLanded()       { isJumping_ = false; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      /// @brief ジャンプ初速の強さ
      float jumpStrength = 5.0f;
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.jumpStrength = jumpStrength;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      if (!std::isfinite(settings.jumpStrength)) settings.jumpStrength = defaults.jumpStrength;
      jumpStrength = settings.jumpStrength;
   }

private:
   float jumpStrength = 5.0f;

private:
   /// @brief ジャンプ中フラグ
   bool isJumping_ = false;
};

} // namespace App
