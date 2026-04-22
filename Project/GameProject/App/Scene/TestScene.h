#pragma once
#include "BaseScene.h"
#include "Model.h"
#include "Camera/Core/VirtualCamera.h"
#include "../Component/GravityFollowCamera.h"
#include "../Component/PlanetLeashCamera.h"
#include <memory>

/// @brief 重力システムテスト用シーン（フェーズ4: GravityFollowCamera）
class TestScene : public GameEngine::BaseScene {
public:
   void Initialize() override;
   void Update() override;
   void Draw() override;

private:
   // 惑星（SphericalGravityAttractorをアタッチ）
   std::unique_ptr<GameEngine::Model> planet_ = nullptr;

   // プレイヤー（GravityBody + PlayerControllerをアタッチ）
   std::unique_ptr<GameEngine::Model> player_ = nullptr;

   // フェーズ4: 重力追従型カメラ用の仮想カメラ
   std::unique_ptr<GameEngine::VirtualCamera> mainVcam_ = nullptr;
   GameEngine::GravityFollowCamera* gravityFollowCamera_ = nullptr; ///< 所有はmainVcam_

   // レアッシュカメラ用の仮想カメラ（優先度を下げて別vcamで管理）
   std::unique_ptr<GameEngine::VirtualCamera> leashVcam_ = nullptr;
   GameEngine::PlanetLeashCamera* leashCamera_ = nullptr; ///< 所有はleashVcam_

   bool useLeashCamera_ = false; ///< Tab キーで切り替え（false=GravityFollow, true=Leash）

   float testTime_ = 0.0f;
};
