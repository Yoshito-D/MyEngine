#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "../Gravity/GravityBody.h"
#include "../Character/CharacterJump.h"
#include "../Camera/GravityFollowCamera.h"
#include "VehicleMover.h"

namespace App {

/// @brief 入力を収集して車の移動・ジャンプへ振り分けるコンポーネント
class VehicleController final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両制御", "Vehicle Controller" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam) { gravityFollowCamera_ = cam; }

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const {
	  return mover_ ? mover_->GetLastMoveDirection() : GameEngine::Vector3{ 0.0f, 0.0f, 1.0f };
   }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief スティック入力デッドゾーン
   float inputDeadZone = 0.3f;

private:
   /// @brief A/D + 左スティック X からステアリング入力（-1〜+1）を収集する
   float CollectSteerInput() const;

   /// @brief W/S + 左スティック Y からピッチ入力（-1〜+1）を収集する
   float CollectPitchInput() const;

   /// @brief ジャンプ入力を収集する
   bool  CollectJumpInput()  const;

   /// @brief ドリフト入力を収集する（Q キー / ゲームパッド LB）
   bool  CollectDriftInput() const;

   /// @brief 依存コンポーネント参照をキャッシュする
   void  CacheComponents();

private:
   /// @brief 車移動コンポーネント
   VehicleMover*  mover_ = nullptr;

   /// @brief ジャンプコンポーネント
   CharacterJump* jump_  = nullptr;

   /// @brief 重力追従カメラ（矢印キー・右スティックで操作）
   GravityFollowCamera* gravityFollowCamera_ = nullptr;
};

} // namespace App
