#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Camera/GravityFollowCamera.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"

namespace App {

/// @brief 入力を収集して車の移動・ジャンプへ振り分けるコンポーネント
class VehicleController final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Control; }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleController";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両制御", "Vehicle Controller"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const;

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 選択中カメラの安定ID。削除されたComponentの生ポインターを保持しない
   std::string gravityFollowCameraId_;
};

} // namespace App
