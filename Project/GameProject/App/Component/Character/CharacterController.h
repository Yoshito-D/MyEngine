#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include "Utility/Math/Vector3.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "../Camera/GravityFollowCamera.h"
#include "../Camera/PlanetLeashCamera.h"
#include "../Gravity/GravityBody.h"
#include "../Camera/ScreenSpaceBasis.h"
#include "../Character/CharacterWalker.h"
#include "../Character/CharacterJump.h"

namespace App {

/// @brief 入力を収集して移動・ジャンプ・カメラ操作へ振り分けるキャラクター制御コンポーネント（現在使用予定はない）
class CharacterController final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "キャラクター制御", "Character Controller" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief 参照カメラを設定し、basisにも即時伝播する
   void SetCamera(GameEngine::Camera* camera) {
	  camera_ = camera;
	  if (basis_) { basis_->SetCamera(camera); }
   }

   /// @brief OrbitalBody参照を設定し、basisにも即時伝播する
   void SetOrbitalBody(GameEngine::OrbitalBody* body) {
	  orbitalBody_ = body;
	  if (basis_) { basis_->SetOrbitalBody(body); }
   }

   /// @brief GravityFollowCamera参照を設定し、basisにも即時伝播する
   void SetGravityFollowCamera(GravityFollowCamera* cam) {
	  gravityFollowCamera_ = cam;
	  if (basis_) { basis_->SetGravityFollowCamera(cam); }
   }

   /// @brief PlanetLeashCamera参照を設定し、basisにも即時伝播する
   void SetPlanetLeashCamera(PlanetLeashCamera* cam) {
	  planetLeashCamera_ = cam;
	  if (basis_) { basis_->SetPlanetLeashCamera(cam); }
   }

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection()    const { return walker_ ? walker_->GetLastMoveDirection() : GameEngine::Vector3{ 0.0f, 0.0f, 0.0f }; }

   /// @brief 直近の前方投影基底を取得する
   GameEngine::Vector3 GetLastForwardProjected() const { return basis_ ? basis_->GetCachedForward() : GameEngine::Vector3{ 0.0f, 0.0f, 1.0f }; }

   /// @brief 直近の右投影基底を取得する
   GameEngine::Vector3 GetLastRightProjected()   const { return basis_ ? basis_->GetCachedRight() : GameEngine::Vector3{ 1.0f, 0.0f, 0.0f }; }

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
   float inputDeadZone = 0.1f;

private:
   /// @brief WASD/左スティックから移動入力を収集する
   GameEngine::Vector2 CollectMoveInput() const;

   /// @brief ジャンプ入力を収集する
   bool                CollectJumpInput() const;

   /// @brief 依存コンポーネント参照をキャッシュする
   void                CacheComponents();

private:
   /// @brief 参照カメラ
   GameEngine::Camera* camera_ = nullptr;

   /// @brief Orbit系カメラ成分
   GameEngine::OrbitalBody* orbitalBody_ = nullptr;

   /// @brief 重力追従カメラ成分
   GravityFollowCamera* gravityFollowCamera_ = nullptr;

   /// @brief レアッシュカメラ成分
   PlanetLeashCamera* planetLeashCamera_ = nullptr;

   /// @brief 画面基底計算コンポーネント
   ScreenSpaceBasis* basis_ = nullptr;

   /// @brief 歩行コンポーネント
   CharacterWalker* walker_ = nullptr;

   /// @brief ジャンプコンポーネント
   CharacterJump* jump_ = nullptr;
};

} // namespace App
