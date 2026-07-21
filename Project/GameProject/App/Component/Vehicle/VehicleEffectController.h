#pragma once

#include "Object/Component/IObjectComponent.h"

namespace App {

/// @brief 車両状態をJSON定義済みのパーティクルスロットへ反映する
class VehicleEffectController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleEffectController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両エフェクト制御", "Vehicle Effect Controller" };

   /// @brief コンポーネント型名を取得する
   /// @return VehicleEffectController
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief ドリフト、ジャンプ、着地状態をパーティクルへ反映する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 設定項目を持たない空JSONを返す
   /// @return 空のJSONオブジェクト
   nlohmann::json Serialize() const override { return nlohmann::json::object(); }

   /// @brief 保存項目がないため何も行わない
   /// @param data 未使用
   void Deserialize(const nlohmann::json& data) override { (void)data; }

#ifdef USE_IMGUI
   /// @brief 現在の遷移検出状態を表示する
   void DrawInspector() override;
#endif

private:
   bool wasJumping_ = false;
   bool wasGrounded_ = true;
};

} // namespace App
