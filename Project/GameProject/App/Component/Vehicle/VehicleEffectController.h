#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include <cstdint>
#include <vector>

namespace App {

/// @brief 車両の通知や状態をパーティクル操作へ対応させる命令の種類。
enum class VehicleEffectRole {
   DriftDust,
   TurboReady,
   TurboBurst,
   TurboFlame,
   Landing,
   LandingFailure,
   Jump
};

/// @brief アセット名から独立した、車両演出の命令とエミッタースロットの対応。
struct VehicleEffectBinding {
   VehicleEffectRole role = VehicleEffectRole::DriftDust; ///< 実行する演出の役割。
   std::int32_t slot = 0;                                 ///< 操作対象のエミッタースロット番号。

   /// @brief 同じ役割とスロットの対応か比較する。
   bool operator==(const VehicleEffectBinding&) const = default;
};

/// @brief 車両イベントに対応する明示的な命令を、設定されたパーティクルスロットへ実行する。
class VehicleEffectController final : public GameEngine::IObjectComponent {
public:
   /// @brief 車両の操作とイベント状態が確定した後に演出を適用する。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Presentation; }

   static constexpr const char* kTypeName = "VehicleEffectController"; ///< コンポーネント型名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "車両エフェクト制御", "Vehicle Effect Controller"}; ///< 表示名。

   /// @brief 保存と登録に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 接続前の通知を既読にして、実行開始前の演出を再生しない。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 各購読者と通知を奪い合わず、継続演出と単発演出を更新する。経過秒数は使用しない。
   void Update(float deltaTime) override;

   /// @brief 役割名とスロット番号の対応をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief 対応表を検証して一括適用する。不正な表では現在の設定を維持する。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーンに依存しない演出の対応表を共通設定として返す。
   nlohmann::json GetSharedParameters() const override { return Serialize(); }

   /// @brief 共通設定の対応表を検証して適用する。
   void ApplySharedParameters(const nlohmann::json& data) override { Deserialize(data); }

#ifdef USE_IMGUI
   /// @brief 明示した演出の役割とスロットの対応を編集する。
   void DrawInspector() override;
#endif

private:
   std::vector<VehicleEffectBinding> bindings_; ///< アセットパスやファイル名を参照しない演出命令。
   VehicleEventCursor eventCursor_;             ///< この演出が独立して保持する通知の既読位置。
};

} // namespace App
