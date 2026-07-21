#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityFollowCamera.h"
#include "PlayerRearFollowCamera.h"
#include "PlanetLeashCamera.h"

namespace App {

/// @brief 自身の位置・重力Up方向をカメラ系コンポーネントへ橋渡しするコンポーネント
class CameraGravityBridge final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CameraGravityBridge";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "カメラ重力ブリッジ", "Camera Gravity Bridge" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief オーナー位置から重力Upを算出し、接続先カメラへ反映する
   void Update(float) override;

   /// @brief JSONに保存されたカメラ名から通知先を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 惑星中心座標を設定する
   void SetPlanetCenter(const GameEngine::Vector3& center)    { planetCenter_        = center; }

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam)      { gravityFollowCamera_ = cam; }

   /// @brief PlayerRearFollowCamera 参照を設定する
   void SetPlayerRearFollowCamera(PlayerRearFollowCamera* cam) { playerRearFollowCamera_ = cam; }

   /// @brief PlanetLeashCamera 参照を設定する
   void SetPlanetLeashCamera(PlanetLeashCamera* cam)          { planetLeashCamera_   = cam; }

public:
   /// @brief 着地時の縦シェイクを有効にするか
   bool enableLandingShake = true;

   /// @brief 着地シェイクの振幅
   float landingShakeAmplitude = 0.15f;

   /// @brief 着地シェイクの周波数
   float landingShakeFrequency = 14.0f;

   /// @brief 着地シェイクの持続時間
   float landingShakeDuration = 0.1f;

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ
   nlohmann::json Serialize() const override;

   /// @brief デシリアライズ
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 重力方向を算出するための惑星中心
   GameEngine::Vector3  planetCenter_        = { 0.0f, 0.0f, 0.0f };

   /// @brief 重力追従カメラへの通知先
   GravityFollowCamera* gravityFollowCamera_ = nullptr;

   /// @brief プレイヤー後方追従カメラへの通知先
   PlayerRearFollowCamera* playerRearFollowCamera_ = nullptr;

   /// @brief レアッシュカメラへの通知先
   PlanetLeashCamera*   planetLeashCamera_   = nullptr;

   /// @brief 着地遷移検出用の前フレーム接地状態
   bool wasGrounded_ = true;

   std::string gravityFollowCameraId_ = "GravityFollowCamera";
   std::string playerRearFollowCameraId_ = "PlayerRearFollowCamera";
   std::string planetLeashCameraId_ = "PlanetLeashCamera";
};

} // namespace App
