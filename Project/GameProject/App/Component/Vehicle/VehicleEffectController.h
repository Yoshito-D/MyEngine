#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstdint>

namespace App {

/// @brief 車両状態をJSON定義済みのパーティクルスロットへ反映する
class VehicleEffectController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleEffectController"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両エフェクト制御", "Vehicle Effect Controller"};

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
   uint64_t lastMiniTurboFireCount_ = 0; ///< カメラが最後に処理したミニターボ発動の連番。
   bool wasJumping_ = false;             ///< 離陸・着地の変化を検出する前フレームのジャンプ状態。
   bool wasGrounded_ = true;             ///< 着地遷移検出用の前フレーム接地状態。
};

} // namespace App
