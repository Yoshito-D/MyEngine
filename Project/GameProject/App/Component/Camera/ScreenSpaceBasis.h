#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "GravityFollowCamera.h"
#include "PlanetLeashCamera.h"

namespace App {

/// @brief スクリーン基準の前後左右を重力平面に投影して移動基底を提供するコンポーネント
class ScreenSpaceBasis final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "ScreenSpaceBasis";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "スクリーンスペース基準", "Screen Space Basis" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief キャッシュ済み前方を重力平面へ再投影して返す
   GameEngine::Vector3 GetForwardBasis(const GameEngine::Vector3& gravityUp) const;

   /// @brief キャッシュ済み右方向を重力平面へ再投影して返す
   GameEngine::Vector3 GetRightBasis(const GameEngine::Vector3& gravityUp) const;

   /// @brief 接続中カメラ情報を使って移動基底キャッシュを更新する
   void UpdateBasis(const GameEngine::Vector3& gravityUp);

   /// @brief 参照カメラを設定する
   void SetCamera(GameEngine::Camera* camera)                     { camera_              = camera; }

   /// @brief OrbitalBody 参照を設定する
   void SetOrbitalBody(GameEngine::OrbitalBody* body)             { orbitalBody_         = body; }

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam)          { gravityFollowCamera_ = cam; }

   /// @brief PlanetLeashCamera 参照を設定する
   void SetPlanetLeashCamera(PlanetLeashCamera* cam)              { planetLeashCamera_   = cam; }

   /// @brief キャッシュ済み前方を取得する
   GameEngine::Vector3 GetCachedForward() const { return cachedForward_; }

   /// @brief キャッシュ済み右方向を取得する
   GameEngine::Vector3 GetCachedRight()   const { return cachedRight_; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ（参照のみのため保存項目なし）
   nlohmann::json Serialize() const override  { return {}; }

   /// @brief デシリアライズ（保存項目なしのため処理なし）
   void Deserialize(const nlohmann::json&) override {}

private:
   /// @brief 指定法線の平面へ射影して正規化する
   static GameEngine::Vector3 ProjectOnPlane(const GameEngine::Vector3& v, const GameEngine::Vector3& normal);

private:
   /// @brief 参照カメラ
   GameEngine::Camera*      camera_              = nullptr;

   /// @brief Orbit系カメラ成分
   GameEngine::OrbitalBody* orbitalBody_         = nullptr;

   /// @brief 重力追従カメラ成分
   GravityFollowCamera*     gravityFollowCamera_ = nullptr;

   /// @brief レアッシュカメラ成分
   PlanetLeashCamera*       planetLeashCamera_   = nullptr;

   /// @brief キャッシュされた前方基底
   mutable GameEngine::Vector3 cachedForward_ = { 0.0f, 0.0f, 1.0f };

   /// @brief キャッシュされた右基底
   mutable GameEngine::Vector3 cachedRight_   = { 1.0f, 0.0f, 0.0f };
};

} // namespace App
