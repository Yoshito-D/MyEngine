#pragma once

#include "GameEngine/Math/Types/Transform.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace GameEngine {
class ParticleEmitterComponent;
}

namespace App {

/// @brief 車両の通知や状態をパーティクル操作へ対応させる命令の種類。
enum class VehicleEffectRole {
   DriftDust,
   TurboReady,
   TurboBurst,
   TurboFlame,
   Landing,
   LandingFailure,
   Jump,
   LandingDebris,
   JumpDebris,
   DriveDust,
   DriveDebris,
   FlightSmoke
};

/// @brief アセット名から独立した、車両演出の命令とエミッタースロットの対応。
struct VehicleEffectBinding {
   VehicleEffectRole role = VehicleEffectRole::DriftDust; ///< 実行する演出の役割。
   std::int32_t slot = 0;                                 ///< 操作対象のエミッタースロット番号。

   /// @brief 同じ役割とスロットの対応か比較する。
   bool operator==(const VehicleEffectBinding&) const = default;
};

/// @brief 車両イベントに応じてパーティクルを操作し、飛行中に煙またはトレイルを放出する。
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

   /// @brief 制御停止時は飛行演出の新規放出を止め、着地失敗の煙の再生を停止する。
   void OnDisable() override;

   /// @brief 各購読者と通知を奪い合わず、継続演出と単発演出を更新する。経過秒数は使用しない。
   void Update(float deltaTime) override;

   /// @brief 役割名とスロット番号の対応、飛行演出の選択をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief 飛行演出の選択を読み込み、対応表は検証に成功した場合に一括適用する。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーンに依存しない演出の対応表を共通設定として返す。
   nlohmann::json GetSharedParameters() const override { return ExtractSharedParameters({"useFlightTrail"}); }

   /// @brief 共通設定の対応表を検証して適用する。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 演出の役割とスロットの対応、飛行演出の選択を編集する。
   void DrawInspector() override;
#endif

private:
   void UpdateFlightTrail(bool isAirborne);
   void UpdateFlightSmoke(GameEngine::ParticleEmitterComponent* emitter, bool isAirborne);
   std::optional<GameEngine::Transform> CalculateFlightSmokePose();

   std::vector<VehicleEffectBinding> bindings_; ///< アセットパスやファイル名を参照しない演出命令。
   VehicleEventCursor eventCursor_;             ///< この演出が独立して保持する通知の既読位置。
   bool wasDriftDustActive_ = false;            ///< 前フレームに接地したドリフトの煙を放出していたか。
   bool wasDriveEffectsActive_ = false;         ///< 前フレームに通常走行の砂埃と小石を放出していたか。
   bool useFlightTrail_ = false;                ///< 飛行煙とトレイルのどちらを使用するかを切り替える。
   std::vector<std::int32_t> activeFlightSmokeSlots_;             ///< 発生中の飛行煙のスロット番号。
};

} // namespace App
