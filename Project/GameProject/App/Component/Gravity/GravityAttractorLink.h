#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameProject/App/Component/Gravity/GravityAttractor.h"
#include <string>

namespace App {

/// @brief 選択済みの重力源を読み、同オーナーのGravityBodyへ一度だけ適用するコンポーネント
class GravityAttractorLink final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Forces; }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "GravityAttractorLink";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"重力アトラクターリンク", "Gravity Attractor Link"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 現在の attractor を GravityBody へ適用する
   void Update(float) override;

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ（参照のみのため保存項目なし。共通保存形式に合わせて空オブジェクトを返す）
   nlohmann::json Serialize() const override { return nlohmann::json::object(); }

   /// @brief デシリアライズ（保存項目なしのため処理なし）
   void Deserialize(const nlohmann::json&) override {}
};

} // namespace App
