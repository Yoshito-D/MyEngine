#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include <cstdint>

namespace App {

/// @brief 一台の車両が発行したイベント連番と、操作更新後に確定した走行状態。
struct VehicleEventSnapshot {
   std::uint64_t takeoffCount = 0;                      ///< 成立したジャンプの連番。
   std::uint64_t landingCount = 0;                      ///< 物理接触で確定した着地の連番。
   std::uint64_t miniTurboCount = 0;                    ///< 実際に加速したミニターボの連番。
   std::uint64_t driftEndCount = 0;                     ///< ドリフト終了の連番。
   bool isAirborne = false;                             ///< 現在ジャンプ中か。
   bool isGrounded = true;                              ///< 接触判定が接地を認め、ジャンプ中ではないか。
   bool isDrifting = false;                             ///< 操作更新後のドリフト状態。
   bool miniTurboReady = false;                         ///< 現在ミニターボを発動可能か。
   bool hasLandingContact = false;                      ///< 直近の着地に有効な接触情報があるか。
   GameEngine::Vector3 landingPoint{};                  ///< 接触点のワールド位置（m）。
   GameEngine::Vector3 landingNormal{0.0f, 1.0f, 0.0f}; ///< 接触面の単位法線。
   LandingResult landingResult = LandingResult::Normal; ///< 同じ着地に対する車両側の品質判定。
};

/// @brief 一つの購読者が前回の観察から新しく受信したイベント。
struct VehicleEventChanges {
   bool tookOff = false;    ///< 新しいジャンプを受信したか。
   bool landed = false;     ///< 新しい着地を受信したか。
   bool miniTurbo = false;  ///< 新しいミニターボを受信したか。
   bool driftEnded = false; ///< 新しいドリフト終了を受信したか。
};

/// @brief 購読者ごとの既読位置。他の購読者の通知を消費しない。
class VehicleEventCursor {
public:
   /// @brief 現在の連番を既読にし、参照の接続前のイベントを再生しない。
   void Reset(const VehicleEventSnapshot& snapshot);

   /// @brief 新着通知を返して既読位置を進める。発行元の連番がリセットされた場合は既読位置だけを戻す。
   VehicleEventChanges Read(const VehicleEventSnapshot& snapshot);

private:
   std::uint64_t takeoff_ = 0, landing_ = 0, miniTurbo_ = 0, driftEnd_ = 0;
};

/// @brief 発生元が発行する通知を車両単位で保持し、操作更新後の状態を全購読者へ公開する。
class VehicleEvents final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleEvents"; ///< コンポーネント型名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両イベント", "Vehicle Events"}; ///< 表示名。

   /// @brief 登録と保存に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 操作の後、各Presentationより前に走行状態を確定する。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Default; }

   /// @brief 新しいシーンの通知履歴と走行状態を初期化する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 操作更新済みの走行状態を確定する。経過秒数は使用しない。
   void Update(float deltaTime) override;

   /// @brief 成立したジャンプを一度発行する。CharacterJumpだけが呼び出す。
   void PublishTakeoff();

   /// @brief 物理接触で確定した着地を発行する。位置はm、法線は単位ベクトル。
   void PublishLanding(const GameEngine::Vector3& point, const GameEngine::Vector3& normal);

   /// @brief 直近の着地に対し、操作フェーズで確定した品質を追記する。
   void SetLandingResult(LandingResult result);

   /// @brief 実際に加速したミニターボを一度発行する。
   void PublishMiniTurbo();

   /// @brief 成立していたドリフトの終了を一度発行する。
   void PublishDriftEnd();

   /// @brief 全購読者が独立して参照できる最新の通知と状態を返す。
   const VehicleEventSnapshot& GetSnapshot() const { return snapshot_; }

   /// @brief 実行履歴は保存せず、空の設定を返す。
   nlohmann::json Serialize() const override { return nlohmann::json::object(); }

   /// @brief 永続設定を持たないため入力を変更しない。
   void Deserialize(const nlohmann::json& data) override { (void)data; }

private:
   VehicleEventSnapshot snapshot_;
};

} // namespace App
